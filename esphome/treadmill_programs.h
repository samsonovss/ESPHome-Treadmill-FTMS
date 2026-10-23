#pragma once
// Bounded custom-plan storage only: no runner and no motor/control writes.
#include "esphome/components/json/json_util.h"
namespace treadmill_recorder {
inline std::atomic<float> plan_speed_min{NAN},plan_speed_max{NAN},plan_speed_step{NAN},plan_incline_min{NAN},plan_incline_max{NAN},plan_incline_step{NAN};
inline std::atomic<uint32_t> plan_gate_ms{0};
inline std::atomic<uint32_t> programs_rev{0};
inline bool plan_stopped(){return plan_gate_ms.load()!=0 && uint32_t(millis()-plan_gate_ms.load())<2500 && !control_busy && !recording;}
inline std::string plan_payload,plan_target;
inline bool plan_id(const std::string &s){if(s.size()!=17||s[0]!='p')return false;for(size_t i=1;i<s.size();i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
inline std::string plan_dir(){return std::string(treadmill_storage::BASE_PATH)+"/programs";}
inline bool plan_grid(double x,float lo,float hi,float step){return std::isfinite(x)&&std::isfinite(lo)&&std::isfinite(hi)&&std::isfinite(step)&&step>0&&x>=lo-0.00001&&x<=hi+0.00001&&fabs((x-lo)/step-round((x-lo)/step))<0.0001;}
inline bool plan_validate(const std::string &body,std::string &id){
 if(body.empty()||body.size()>8192)return false;
 return esphome::json::parse_json(body,[&](JsonObject root)->bool{
  if(root.size()<5||root.size()>8||!root["version"].is<int>()||root["version"].as<int>()!=1||!root["id"].is<const char *>()||!root["name"].is<const char *>()||!root["schema_units"].is<JsonObject>()||!root["blocks"].is<JsonArray>())return false;
  id=root["id"].as<std::string>();if(!plan_id(id))return false;
  std::string name=root["name"].as<std::string>();if(name.empty()||name.size()>80)return false;bool nonspace=false;for(unsigned char c:name){if(c<32)return false;if(c!=' ')nonspace=true;}if(!nonspace)return false;
  // Validate UTF-8, including overlong encodings, surrogates and > U+10FFFF.
  for(size_t i=0;i<name.size();){unsigned char c=name[i++];if(c<128)continue;unsigned n;uint32_t v,min;if(c>=0xc2&&c<=0xdf){n=1;v=c&31;min=128;}else if(c>=0xe0&&c<=0xef){n=2;v=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){n=3;v=c&7;min=65536;}else return false;while(n--){if(i>=name.size())return false;c=name[i++];if((c&0xc0)!=0x80)return false;v=(v<<6)|(c&63);}if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;}
  {size_t known=5;for(const char *k:{"goal","plan","day"}){if(root[k].isNull())continue;if(!root[k].is<const char *>())return false;std::string v=root[k].as<std::string>();size_t mx=strcmp(k,"day")==0?10:40;if(v.empty()||v.size()>mx)return false;for(unsigned char c:v)if(c<32)return false;if(strcmp(k,"day")==0){for(size_t i=0;i<10;i++){if(i==4||i==7){if(v[i]!='-')return false;}else if(v[i]<'0'||v[i]>'9')return false;}}++known;}if(root.size()!=known)return false;}
  JsonObject units=root["schema_units"];if(units.size()!=3||!units["duration"].is<const char *>()||!units["speed"].is<const char *>()||!units["incline"].is<const char *>()||strcmp(units["duration"].as<const char *>(),"s")||strcmp(units["speed"].as<const char *>(),"km/h")||strcmp(units["incline"].as<const char *>(),"level"))return false;
  JsonArray blocks=root["blocks"];if(blocks.size()<1||blocks.size()>64)return false;uint32_t count=0,time=0;
  for(JsonObject b:blocks){if(b.isNull()||b.size()!=2||!b["repeat"].is<int>()||!b["steps"].is<JsonArray>())return false;int repeat=b["repeat"];if(repeat<1||repeat>20)return false;JsonArray steps=b["steps"];if(steps.size()<1||steps.size()>64)return false;for(JsonObject s:steps){if(s.isNull()||s.size()!=3||!s["duration"].is<int>()||!s["incline"].is<double>())return false;int duration=s["duration"];if(duration<1||duration>14400||!plan_grid(s["incline"],plan_incline_min,plan_incline_max,plan_incline_step))return false;if(!s["zone"].isNull()){if(!s["zone"].is<int>()||int(s["zone"])<1||int(s["zone"])>5)return false;}else if(!s["speed"].is<double>()||!plan_grid(s["speed"],plan_speed_min,plan_speed_max,plan_speed_step))return false;count+=repeat;time+=uint32_t(duration)*repeat;if(count>128||time>14400)return false;}}
  return true;
 });
}
inline bool plan_write(){
 std::string validated;if(!plan_stopped()||!plan_validate(plan_payload,validated)||validated!=plan_target){job_error="stopped_or_traits_validation_failed";return false;}
 const std::string dir=plan_dir();if(mkdir(dir.c_str(),0700)!=0&&errno!=EEXIST){job_error="program_directory_failed";return false;}
 // Fixed directory bound: no hidden unbounded collection or automatic erasure.
 DIR *d=opendir(dir.c_str());if(!d){job_error="program_directory_failed";return false;}size_t count=0;bool existing=false;while(auto *entry=readdir(d)){std::string n=entry->d_name;if(n.size()==22&&n.substr(17)==".json"&&plan_id(n.substr(0,17))){count++;if(n==plan_target+".json")existing=true;}}closedir(d);if(!existing&&count>=64){job_error="program_limit_64";return false;}
 const std::string path=dir+"/"+plan_target+".json",tmp=path+".tmp";
 FILE *f=fopen(tmp.c_str(),"wb");if(!f){job_error="program_open_failed_or_full";return false;}
 bool ok=fwrite(plan_payload.data(),1,plan_payload.size(),f)==plan_payload.size();if(fflush(f)!=0)ok=false;if(fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;
 if(!ok){job_error="program_write_failed_or_full";return false;}
 if(!plan_stopped()){job_error="workout_active_or_finalizing";return false;}
 if(rename(tmp.c_str(),path.c_str())!=0){job_error="program_commit_failed";return false;}job_error.clear();++programs_rev;return true;
}
inline const char *plan_delete(const std::string &id){
 if(!plan_id(id))return "invalid_program_id";
 if(!plan_stopped())return "fresh_stop_required";
 const std::string path=plan_dir()+"/"+id+".json";struct stat st{};
 if(stat(path.c_str(),&st)!=0)return errno==ENOENT?"program_not_found":"program_stat_failed";
 if(unlink(path.c_str())!=0)return "program_delete_failed";
 ++programs_rev;return nullptr;
}
class ProgramHandler:public AsyncWebHandler {
 // IDF handles HTTP callbacks on one task. A bounded member buffer avoids request
 // temporary ownership APIs and leaks on an interrupted upload; next index=0 resets.
 std::string body_;httpd_req_t *body_request_=nullptr;bool body_valid_=false;
 public:
 bool canHandle(AsyncWebServerRequest *r)const override{std::unique_ptr<char[]> b(new(std::nothrow)char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!b)return false;auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE));return (r->method()==HTTP_GET&&(u=="/api/programs/list"||u=="/api/programs/file"))||(r->method()==HTTP_POST&&(u=="/api/programs/save"||u=="/api/programs/delete"));}
 void handleBody(AsyncWebServerRequest *r,uint8_t *data,size_t len,size_t index,size_t total)override{if(index==0){body_.clear();body_request_=*r;body_valid_=total>0&&total<=8192;if(body_valid_)body_.reserve(total);}if(!body_valid_||body_request_!=static_cast<httpd_req_t *>(*r)||index!=body_.size()||len>total-index){body_valid_=false;return;}body_.append(reinterpret_cast<char *>(data),len);}
 void reply(AsyncWebServerRequest *r,int code,const std::string &s){auto *res=r->beginResponse(code,"application/json",s);res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);}
 void handleRequest(AsyncWebServerRequest *r)override{
  std::unique_ptr<char[]> uri(new(std::nothrow)char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!uri){reply(r,503,"{\"error\":\"memory_unavailable\"}");return;}auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(uri.get(),AsyncWebServerRequest::URL_BUF_SIZE));FsLease lease;if(!lease.owned||busy()){reply(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}if(!initialized||!treadmill_storage::mounted||!queue){reply(r,503,"{\"error\":\"storage_not_ready\"}");return;}
  if(u=="/api/programs/save"){
   httpd_req_t *raw=*r;auto ct=r->get_header("Content-Type");std::string id;
   if(strchr(raw->uri,'?')||!ct.has_value()||ct.value()!="application/json"||!body_valid_||body_request_!=raw||body_.size()!=r->contentLength()||!plan_validate(body_,id)){body_.clear();body_valid_=false;reply(r,400,"{\"error\":\"invalid_program_or_traits\"}");return;}
   if(!plan_stopped()){body_.clear();body_valid_=false;reply(r,409,"{\"error\":\"fresh_stop_required\"}");return;}
   plan_payload=std::move(body_);body_valid_=false;plan_target=id;Event e{};e.kind=6;mutation_pending=true;job_id=id;job_operation="program_save";job_error.clear();job_state=1;
   if(xQueueSend(queue,&e,0)!=pdTRUE){mutation_pending=false;job_state=0;plan_payload.clear();reply(r,503,"{\"error\":\"queue_unavailable\"}");return;}++job_number;if(!job_number)++job_number;reply(r,202,"{\"job\":"+std::to_string(job_number.load())+",\"state\":\"queued\"}");return;
  }
  if(u=="/api/programs/delete"){
   if(r->method()!=HTTP_POST){reply(r,405,"{\"error\":\"POST only\"}");return;}
   httpd_req_t *raw=*r;const char *q=strchr(raw->uri,'?');if(!q||strncmp(q+1,"id=",3)!=0||strlen(q+4)!=17){reply(r,400,"{\"error\":\"invalid_program_id\"}");return;}
   const char *e=plan_delete(std::string(q+4));if(e){std::string es=e;reply(r,es=="program_not_found"?404:(es=="fresh_stop_required"?409:(es=="invalid_program_id"?400:500)),"{\"error\":\""+es+"\"}");return;}
   reply(r,200,"{\"deleted\":true}");return;
  }
  if(u=="/api/programs/list"){
   std::string out="{\"items\":[";DIR *d=opendir(plan_dir().c_str());if(!d&&errno!=ENOENT){reply(r,500,"{\"error\":\"program_list_failed\"}");return;}size_t count=0,scanned=0;if(d){while(auto *entry=readdir(d)){if(++scanned>160){closedir(d);reply(r,500,"{\"error\":\"program_directory_limit\"}");return;}std::string n=entry->d_name;if(n.size()!=22||n.substr(17)!=".json"||!plan_id(n.substr(0,17)))continue;if(count>=64)break;if(count++)out+=',';out+="{\"id\":"+json_string(n.substr(0,17).c_str())+"}";}closedir(d);}reply(r,200,out+"]}");return;
  }
  httpd_req_t *raw=*r;const char *q=strchr(raw->uri,'?');if(!q||strncmp(q+1,"id=",3)!=0||strlen(q+4)!=17||!plan_id(q+4)){reply(r,400,"{\"error\":\"invalid_program_id\"}");return;}
  std::string path=plan_dir()+"/"+std::string(q+4)+".json";struct stat st{};if(stat(path.c_str(),&st)!=0){reply(r,errno==ENOENT?404:500,"{\"error\":\"program_read_failed\"}");return;}if(st.st_size<1||st.st_size>8192){reply(r,500,"{\"error\":\"program_file_size\"}");return;}
  FILE *f=fopen(path.c_str(),"rb");if(!f){reply(r,500,"{\"error\":\"program_read_failed\"}");return;}std::string out;out.resize(st.st_size);bool ok=fread(out.data(),1,out.size(),f)==out.size();if(fclose(f)!=0)ok=false;if(!ok){reply(r,500,"{\"error\":\"program_read_failed\"}");return;}reply(r,200,out);
 }
};
inline ProgramHandler program_handler;
}
