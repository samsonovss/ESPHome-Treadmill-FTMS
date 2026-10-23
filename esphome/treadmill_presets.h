#pragma once
// User preset lists (pulse / hiit / fartlek): one small JSON file per kind on the SD card, whole-list read/replace.
// Same rules as the program store: only with the belt stopped and no workout being recorded.
#include "esphome/components/json/json_util.h"
namespace treadmill_recorder {
inline std::atomic<uint32_t> presets_rev{0};
inline std::string preset_dir(){return std::string(treadmill_storage::BASE_PATH)+"/presets";}
inline const char *preset_kind(const char *uri){
 const char *q=strchr(uri,'?');if(!q||strncmp(q+1,"kind=",5)!=0)return nullptr;q+=6;
 for(const char *k:{"pulse","hiit","fartlek"})if(strcmp(q,k)==0)return k;
 return nullptr;
}
inline bool preset_validate(const std::string &body){
 if(body.empty()||body.size()>8192)return false;
 return esphome::json::parse_json(body,[&](JsonObject root)->bool{
  if(root.size()!=1||!root["items"].is<JsonArray>())return false;
  JsonArray items=root["items"];if(items.size()>24)return false;
  for(JsonObject it:items){
   if(it.isNull()||!it["name"].is<const char *>())return false;
   std::string n=it["name"].as<std::string>();if(n.empty()||n.size()>60)return false;
   for(unsigned char c:n)if(c<32)return false;
  }
  return true;
 });
}
class PresetHandler:public AsyncWebHandler {
 std::string body_;httpd_req_t *body_request_=nullptr;bool body_valid_=false;
 public:
 bool canHandle(AsyncWebServerRequest *r)const override{
  std::unique_ptr<char[]> b(new(std::nothrow)char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!b)return false;
  auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE));
  return u=="/api/presets"&&(r->method()==HTTP_GET||r->method()==HTTP_POST);
 }
 void handleBody(AsyncWebServerRequest *r,uint8_t *data,size_t len,size_t index,size_t total)override{
  if(index==0){body_.clear();body_request_=*r;body_valid_=total>0&&total<=8192;if(body_valid_)body_.reserve(total);}
  if(!body_valid_||body_request_!=static_cast<httpd_req_t *>(*r)||index!=body_.size()||len>total-index){body_valid_=false;return;}
  body_.append(reinterpret_cast<char *>(data),len);
 }
 void reply(AsyncWebServerRequest *r,int code,const std::string &s){
  auto *res=r->beginResponse(code,"application/json",s);res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);
 }
 void handleRequest(AsyncWebServerRequest *r)override{
  httpd_req_t *raw=*r;const char *kind=preset_kind(raw->uri);
  if(!kind){body_.clear();body_valid_=false;reply(r,400,"{\"error\":\"invalid_kind\"}");return;}
  FsLease lease;if(!lease.owned||busy()){body_.clear();body_valid_=false;reply(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}
  if(!initialized||!treadmill_storage::mounted){body_.clear();body_valid_=false;reply(r,503,"{\"error\":\"storage_not_ready\"}");return;}
  const std::string dir=preset_dir(),path=dir+"/"+kind+".json",tmp=path+".tmp";
  if(r->method()==HTTP_GET){
   struct stat st{};if(stat(path.c_str(),&st)!=0){reply(r,errno==ENOENT?200:500,errno==ENOENT?"{\"items\":[]}":"{\"error\":\"preset_stat_failed\"}");return;}
   if(st.st_size<1||st.st_size>8192){reply(r,500,"{\"error\":\"preset_file_size\"}");return;}
   FILE *f=fopen(path.c_str(),"rb");if(!f){reply(r,500,"{\"error\":\"preset_read_failed\"}");return;}
   std::string out;out.resize(st.st_size);bool ok=fread(out.data(),1,out.size(),f)==out.size();if(fclose(f)!=0)ok=false;
   if(!ok){reply(r,500,"{\"error\":\"preset_read_failed\"}");return;}reply(r,200,out);return;
  }
  auto ct=r->get_header("Content-Type");
  if(!ct.has_value()||ct.value()!="application/json"||!body_valid_||body_request_!=raw||body_.size()!=r->contentLength()||!preset_validate(body_)){body_.clear();body_valid_=false;reply(r,400,"{\"error\":\"invalid_presets\"}");return;}
  std::string payload=std::move(body_);body_.clear();body_valid_=false;
  if(mkdir(dir.c_str(),0700)!=0&&errno!=EEXIST){reply(r,500,"{\"error\":\"preset_directory_failed\"}");return;}
  FILE *f=fopen(tmp.c_str(),"wb");if(!f){reply(r,500,"{\"error\":\"preset_open_failed_or_full\"}");return;}
  bool ok=fwrite(payload.data(),1,payload.size(),f)==payload.size();if(fflush(f)!=0)ok=false;if(fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;
  if(!ok){unlink(tmp.c_str());reply(r,500,"{\"error\":\"preset_write_failed_or_full\"}");return;}
  if(rename(tmp.c_str(),path.c_str())!=0){unlink(tmp.c_str());reply(r,500,"{\"error\":\"preset_commit_failed\"}");return;}
  ++presets_rev;reply(r,200,"{\"saved\":true}");
 }
};
inline PresetHandler preset_handler;

// Display (Nextion list pages) side: read-only snapshot of one preset kind, seed markers and invalid items skipped.
struct NxPre{std::string name,plan;int hi=60,lo=60,cycles=8;float warm=3,cool=3,low=NAN,high=NAN;bool manual=false,hasWC=false;};
inline std::vector<NxPre> nx_pre_items;
inline int nx_pre_off=0;
inline bool nx_pre_load(const char *kind){
 nx_pre_items.clear();nx_pre_off=0;
 FsLease lease;if(!lease.owned||!treadmill_storage::mounted||!initialized)return false;
 const std::string path=preset_dir()+"/"+kind+".json";struct stat st{};
 if(stat(path.c_str(),&st)!=0||st.st_size<1||st.st_size>8192)return false;
 FILE *f=fopen(path.c_str(),"rb");if(!f)return false;
 std::string b;b.resize(st.st_size);bool ok=fread(b.data(),1,b.size(),f)==b.size();fclose(f);if(!ok)return false;
 return esphome::json::parse_json(b,[&](JsonObject root)->bool{
  if(!root["items"].is<JsonArray>())return false;
  JsonArray arr=root["items"];for(JsonObject it:arr){
   if(it.isNull()||!it["name"].is<const char *>()||it["seed"].as<int>()!=0)continue;
   NxPre p;p.name=it["name"].as<std::string>();
   if(it["plan"].is<const char *>())p.plan=it["plan"].as<std::string>();
   if(it["hi"].is<int>())p.hi=it["hi"].as<int>();
   if(it["lo"].is<int>())p.lo=it["lo"].as<int>();
   if(it["cycles"].is<int>())p.cycles=it["cycles"].as<int>();
   if(it["warm"].is<float>()){p.warm=it["warm"].as<float>();p.hasWC=true;}
   if(it["cool"].is<float>()){p.cool=it["cool"].as<float>();p.hasWC=true;}
   if(it["low"].is<float>())p.low=it["low"].as<float>();
   if(it["high"].is<float>())p.high=it["high"].as<float>();
   p.manual=it["hiitManual"].as<bool>();
   nx_pre_items.push_back(std::move(p));
   if(nx_pre_items.size()>=24)break;
  }
  return true;
 });
}
}
