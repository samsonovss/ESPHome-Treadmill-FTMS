#pragma once
// Route library (opaque UI JSON files on LittleFS) + one "Custom route" incline slot.
// No motor writes here: update_incline_by_map reads the slot only while the belt runs;
// every HTTP write requires a fresh STOP gate (plan_stopped) and the archive lease.
#include "treadmill_recorder.h"
#include <dirent.h>
#include <algorithm>
#include <vector>
namespace treadmill_routes {
using namespace treadmill_recorder;
inline constexpr size_t ROUTE_FILE_MAX = 65536, ROUTE_FILES_MAX = 200, ACTIVE_BODY_MAX = 24576;
inline constexpr int ACTIVE_MAX_POINTS = 1024;
inline constexpr const char *CUSTOM_NAME = "Custom route";
// Active slot in PSRAM: [km, level] pairs. Published length 0 while being replaced.
inline float (*points)[2] = nullptr;
inline std::atomic<int> count{0};
inline std::string active_name; // only touched on the HTTP task
inline std::string dir(){return std::string(treadmill_storage::BASE_PATH)+"/routes";}
inline bool route_id(const char *s){if(strlen(s)!=17||s[0]!='r')return false;for(int i=1;i<17;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
inline bool utf8_name(const std::string &name){
  if(name.empty()||name.size()>80)return false;
  for(size_t i=0;i<name.size();){unsigned char c=name[i++];if(c<32||c==127)return false;if(c<128)continue;unsigned n;uint32_t v,min;if(c>=0xc2&&c<=0xdf){n=1;v=c&31;min=128;}else if(c>=0xe0&&c<=0xef){n=2;v=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){n=3;v=c&7;min=65536;}else return false;while(n--){if(i>=name.size())return false;c=name[i++];if((c&0xc0)!=0x80)return false;v=(v<<6)|(c&63);}if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;}
  return true;
}
using PsString = std::basic_string<char,std::char_traits<char>,PsramAllocator<char>>;
// Body format (text/plain): "TREADMILL-ROUTE 1\n<name>\n<km> <level>\n..." km strictly increasing from 0.
inline bool apply_active(const PsString &body){
  if(!points){points=static_cast<float(*)[2]>(heap_caps_malloc(sizeof(float)*4*ACTIVE_MAX_POINTS,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!points)return false;}
  float (*staging)[2]=points+ACTIVE_MAX_POINTS; // second half: validated fully before publishing
  const char *p=body.c_str(),*end=p+body.size();
  const char *nl=static_cast<const char*>(memchr(p,'\n',end-p));if(!nl||std::string(p,nl)!="TREADMILL-ROUTE 1")return false;p=nl+1;
  nl=static_cast<const char*>(memchr(p,'\n',end-p));if(!nl)return false;std::string name(p,nl);if(!utf8_name(name))return false;p=nl+1;
  const float lo=plan_incline_min,hi=plan_incline_max;
  int n=0;float prev=-1;
  while(p<end){
    nl=static_cast<const char*>(memchr(p,'\n',end-p));const char *le=nl?nl:end;
    if(le==p){p=le+1;continue;}
    if(n>=ACTIVE_MAX_POINTS||le-p>40)return false;
    char line[48];memcpy(line,p,le-p);line[le-p]=0;char *e1,*e2;
    float km=strtof(line,&e1);if(e1==line||*e1!=' ')return false;
    float level=strtof(e1+1,&e2);if(e2==e1+1||*e2!=0)return false;
    if(!std::isfinite(km)||km<0||km>100||(n==0&&km!=0)||km<=prev)return false;
    if(!plan_grid(level,lo,hi,0.1f))return false; // tenths: smooth routes (10.10); range still from Set Incline
    staging[n][0]=km;staging[n][1]=level;prev=km;n++;p=le+(nl?1:0);
  }
  if(n<2)return false;
  count=0;memcpy(points,staging,sizeof(float)*2*n);count=n;active_name=name;return true;
}

// Display sidecars: <id>.act holds the ready-to-apply active body (computed by the web with calibration).
inline std::string act_path(const std::string &id){return dir()+"/"+id+".act";}
struct ActItem{std::string id,name;float km;};
inline bool read_act(const std::string &id,PsString &out){
  struct stat st{};const std::string p=act_path(id);if(stat(p.c_str(),&st)!=0||st.st_size<20||size_t(st.st_size)>ACTIVE_BODY_MAX)return false;
  out.resize(st.st_size);FILE *f=fopen(p.c_str(),"rb");if(!f)return false;bool ok=fread(out.data(),1,out.size(),f)==out.size();if(fclose(f)!=0)ok=false;return ok;}
inline bool act_meta(const PsString &b,std::string &name,float &km){
  const char *p=b.c_str(),*end=p+b.size();const char *nl=static_cast<const char*>(memchr(p,'\n',end-p));if(!nl)return false;p=nl+1;
  nl=static_cast<const char*>(memchr(p,'\n',end-p));if(!nl)return false;name.assign(p,nl);
  const char *q=end;while(q>p&&q[-1]=='\n')q--;const char *ls=q;while(ls>nl&&ls[-1]!='\n')ls--;
  km=strtof(ls,nullptr);return !name.empty();}
inline std::string utf8_cut(const std::string &s,size_t max){if(s.size()<=max)return s;size_t n=max;while(n>0&&(static_cast<unsigned char>(s[n])&0xc0)==0x80)n--;return s.substr(0,n);}
inline std::vector<ActItem> nx_items;
inline int nx_off=0;
// Rebuilds nx_items from sidecars; returns the Nextion TextSelect path ("Name · 5.2 km" joined by the two characters backslash-r, which Nextion turns into a line break).
inline std::string nx_build(){
  nx_items.clear();std::string path;FsLease lease;if(!lease.owned||!treadmill_storage::mounted)return path;
  DIR *dd=opendir(dir().c_str());if(!dd)return path;
  while(auto *e=readdir(dd)){std::string f=e->d_name;if(f.size()!=21||f.substr(17)!=".act"||!route_id(f.substr(0,17).c_str()))continue;
    if(nx_items.size()>=40)break;PsString b;ActItem it;it.id=f.substr(0,17);if(!read_act(it.id,b)||!act_meta(b,it.name,it.km))continue;nx_items.push_back(it);}
  closedir(dd);
  std::sort(nx_items.begin(),nx_items.end(),[](const ActItem&a,const ActItem&b){return a.name<b.name;});
  size_t shown=0;for(auto &it:nx_items){char kb[24];snprintf(kb,sizeof kb," \xc2\xb7 %.1f km",it.km);std::string line=utf8_cut(it.name,34)+kb;if(path.size()+line.size()+1>900)break;for(auto &ch:line)if(ch=='"'||ch=='\\')ch='\'';if(shown++)path+="\\r";path+=line;}
  nx_items.resize(shown);return path;}
inline bool nx_open(int idx){
  if(idx<0||idx>=int(nx_items.size())||!plan_stopped())return false;
  FsLease lease;if(!lease.owned)return false;PsString b;if(!read_act(nx_items[idx].id,b))return false;return apply_active(b);}
class RouteHandler:public AsyncWebHandler {
  PsString body_;httpd_req_t *body_request_=nullptr;bool body_valid_=false;size_t body_max_=0;
 public:
  bool canHandle(AsyncWebServerRequest *r)const override{std::unique_ptr<char[]> b(new(std::nothrow)char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!b)return false;auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE));
    return (r->method()==HTTP_GET&&(u=="/api/routes/list"||u=="/api/routes/file"||u=="/api/route/active"))||(r->method()==HTTP_POST&&(u=="/api/routes/save"||u=="/api/routes/act"||u=="/api/routes/delete"||u=="/api/route/active"));}
  void handleBody(AsyncWebServerRequest *r,uint8_t *data,size_t len,size_t index,size_t total)override{
    if(index==0){body_.clear();body_request_=*r;body_max_=ROUTE_FILE_MAX;body_valid_=total>0&&total<=body_max_;if(body_valid_)body_.reserve(total);}
    if(!body_valid_||body_request_!=static_cast<httpd_req_t *>(*r)||index!=body_.size()||len>total-index){body_valid_=false;return;}
    body_.append(reinterpret_cast<char *>(data),len);}
  void reply(AsyncWebServerRequest *r,int code,const std::string &s){auto *res=r->beginResponse(code,"application/json",s);res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);}
  void drop(){body_.clear();body_.shrink_to_fit();body_valid_=false;}
  void handleRequest(AsyncWebServerRequest *r)override{
    std::unique_ptr<char[]> uri(new(std::nothrow)char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!uri){drop();reply(r,503,"{\"error\":\"memory_unavailable\"}");return;}
    auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(uri.get(),AsyncWebServerRequest::URL_BUF_SIZE));
    httpd_req_t *raw=*r;const char *q=strchr(raw->uri,'?');
    const bool post=r->method()==HTTP_POST;
    if(u=="/api/route/active"){
      if(!post){std::string out="{\"name\":";out+=json_string(count.load()?active_name.c_str():"");out+=",\"option\":";out+=json_string(CUSTOM_NAME);out+=",\"points\":[";
        char b2[40];const int n=count.load();for(int i=0;i<n;i++){snprintf(b2,sizeof b2,"%s[%.4f,%g]",i?",":"",points[i][0],points[i][1]);out+=b2;}out+="]}";reply(r,200,out);return;}
      if(q||!body_valid_||body_request_!=raw||body_.size()!=r->contentLength()||body_.size()>ACTIVE_BODY_MAX){drop();reply(r,400,"{\"error\":\"invalid_route\"}");return;}
      if(!plan_stopped()){drop();reply(r,409,"{\"error\":\"fresh_stop_required\"}");return;}
      const bool ok=apply_active(body_);drop();
      if(!ok){reply(r,400,"{\"error\":\"invalid_route_or_traits\"}");return;}
      reply(r,200,"{\"ok\":true,\"points\":"+std::to_string(count.load())+"}");return;
    }
    FsLease lease;if(!lease.owned){drop();reply(r,409,"{\"error\":\"archive_busy\"}");return;}
    if(!initialized||!treadmill_storage::mounted){drop();reply(r,503,"{\"error\":\"storage_not_ready\"}");return;}
    const std::string d=dir();
    if(u=="/api/routes/list"){
      std::string out="{\"items\":[";DIR *dd=opendir(d.c_str());if(!dd&&errno!=ENOENT){reply(r,500,"{\"error\":\"route_list_failed\"}");return;}
      size_t n=0,scanned=0;if(dd){while(auto *e=readdir(dd)){if(++scanned>400)break;std::string f=e->d_name;if(f.size()!=22||f.substr(17)!=".json"||!route_id(f.substr(0,17).c_str()))continue;if(n>=ROUTE_FILES_MAX)break;struct stat st{};if(stat((d+"/"+f).c_str(),&st)!=0)continue;if(n++)out+=',';out+="{\"id\":"+json_string(f.substr(0,17).c_str())+",\"size\":"+std::to_string((long)st.st_size)+"}";}closedir(dd);}
      reply(r,200,out+"]}");return;
    }
    if(!q||strncmp(q+1,"id=",3)!=0||!route_id(q+4)){drop();reply(r,400,"{\"error\":\"invalid_route_id\"}");return;}
    const std::string path=d+"/"+std::string(q+4)+".json";
    if(u=="/api/routes/file"){
      struct stat st{};if(stat(path.c_str(),&st)!=0){reply(r,errno==ENOENT?404:500,"{\"error\":\"route_read_failed\"}");return;}
      if(st.st_size<2||size_t(st.st_size)>ROUTE_FILE_MAX){reply(r,500,"{\"error\":\"route_file_size\"}");return;}
      PsString out;out.resize(st.st_size);FILE *f=fopen(path.c_str(),"rb");if(!f){reply(r,500,"{\"error\":\"route_read_failed\"}");return;}
      bool ok=fread(out.data(),1,out.size(),f)==out.size();if(fclose(f)!=0)ok=false;if(!ok){reply(r,500,"{\"error\":\"route_read_failed\"}");return;}
      auto *res=r->beginResponse(200,"application/json",reinterpret_cast<const uint8_t*>(out.data()),out.size());res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);return;
    }
    // Mutations: stopped belt only, never while a workout is recorded or finalized.
    if(busy()||!plan_stopped()){drop();reply(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}
    if(u=="/api/routes/delete"){
      if(r->contentLength()!=0){drop();reply(r,400,"{\"error\":\"unexpected_body\"}");return;}
      if(unlink(path.c_str())!=0){reply(r,errno==ENOENT?404:500,"{\"error\":\"route_delete_failed\"}");return;}
      unlink(act_path(std::string(q+4)).c_str());capacity();reply(r,200,"{\"ok\":true}");return;
    }
    if(u=="/api/routes/act"){
      const bool bok=body_valid_&&body_request_==raw&&body_.size()==r->contentLength();PsString tst;if(bok)tst=body_;drop();
      if(!bok||tst.size()<20||tst.size()>ACTIVE_BODY_MAX||tst.compare(0,18,"TREADMILL-ROUTE 1\n")!=0){reply(r,400,"{\"error\":\"invalid_act\"}");return;}
      if(mkdir(d.c_str(),0700)!=0&&errno!=EEXIST){reply(r,500,"{\"error\":\"route_directory_failed\"}");return;}
      struct stat jst{};if(stat(path.c_str(),&jst)!=0){reply(r,404,"{\"error\":\"route_not_found\"}");return;}
      const std::string ap=act_path(std::string(q+4)),tp=ap+".tmp";FILE *f=fopen(tp.c_str(),"wb");if(!f){reply(r,500,"{\"error\":\"route_open_failed\"}");return;}
      bool ok=fwrite(tst.data(),1,tst.size(),f)==tst.size();if(fflush(f)!=0)ok=false;if(fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;
      if(!ok||rename(tp.c_str(),ap.c_str())!=0){unlink(tp.c_str());reply(r,500,"{\"error\":\"route_write_failed\"}");return;}
      reply(r,200,"{\"ok\":true}");return;
    }
    // save
    auto ct=r->get_header("Content-Type");
    if(!ct.has_value()||ct.value()!="application/json"||!body_valid_||body_request_!=raw||body_.size()!=r->contentLength()||body_.size()<2||body_.front()!='{'||body_.back()!='}'){drop();reply(r,400,"{\"error\":\"invalid_route_file\"}");return;}
    if(mkdir(d.c_str(),0700)!=0&&errno!=EEXIST){drop();reply(r,500,"{\"error\":\"route_directory_failed\"}");return;}
    {DIR *dd=opendir(d.c_str());size_t n=0;bool existing=false;if(dd){while(auto *e=readdir(dd)){std::string f=e->d_name;if(f.size()==22&&f.substr(17)==".json"){n++;if(f==std::string(q+4)+".json")existing=true;}}closedir(dd);}
     if(!existing&&n>=ROUTE_FILES_MAX){drop();reply(r,507,"{\"error\":\"route_limit_200\"}");return;}}
    size_t total=0,used=0;if(esp_littlefs_info(treadmill_storage::PARTITION,&total,&used)!=ESP_OK||total-used<body_.size()+262144){drop();reply(r,507,"{\"error\":\"storage_full\"}");return;}
    const std::string tmp=path+".tmp";FILE *f=fopen(tmp.c_str(),"wb");if(!f){drop();reply(r,500,"{\"error\":\"route_open_failed\"}");return;}
    bool ok=fwrite(body_.data(),1,body_.size(),f)==body_.size();if(fflush(f)!=0)ok=false;if(fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;drop();
    if(!ok||rename(tmp.c_str(),path.c_str())!=0){unlink(tmp.c_str());reply(r,500,"{\"error\":\"route_write_failed\"}");return;}
    capacity();reply(r,200,"{\"ok\":true}");
  }
};
inline RouteHandler handler;
inline void register_handler(){if(esphome::web_server_base::global_web_server_base)esphome::web_server_base::global_web_server_base->add_handler(&handler);}
} // namespace treadmill_routes
