#pragma once
#include "profile_core.h"
#include "esphome/components/json/json_util.h"
#include <functional>
#include <mutex>
#include <esp_random.h>
// Included after recorder definitions. All state and file access require FsLease.
namespace treadmill_profiles {
inline std::vector<Profile> items;
inline std::string active,error;
inline bool ready=false;
// Main-loop-only applied owner snapshot: recorder start must not depend on FS lock availability.
inline char applied_owner_id[25]{},applied_owner_name[61]{};
inline void snapshot_owner(const Profile &p){snprintf(applied_owner_id,sizeof applied_owner_id,"%s",p.id.c_str());snprintf(applied_owner_name,sizeof applied_owner_name,"%s",p.name.c_str());}
inline bool pending_apply=false;
inline bool gate_locked=true;
// A new boot gets a new CAS generation; pre-reboot browsers cannot overwrite restored data.
inline const uint32_t generation=esp_random();
inline uint32_t revision=1,applied_revision=1;
inline std::vector<Profile> queued;
inline std::string queued_active;
inline std::function<Profile()> capture;
inline std::function<void(const Profile&)> apply;
inline std::function<bool()> locked;
inline std::string path(){return std::string(treadmill_storage::BASE_PATH)+"/profiles.json";}
inline std::string library_owner(){return ready&&!pending_apply?active:std::string();}
inline std::string library_default_owner(){for(const auto&p:items)if(p.id=="p1")return p.id;return items.empty()?std::string():items.front().id;}
inline Profile *current(){for(auto &p:items)if(p.id==active)return &p;return nullptr;}
inline std::string encode(const std::vector<Profile>&list,const std::string &selected){
 return esphome::json::build_json([&](JsonObject root){root["version"]=1;root["active"]=selected;JsonArray a=root["items"].to<JsonArray>();for(const auto&p:list){JsonObject o=a.add<JsonObject>();o["id"]=p.id;o["name"]=p.name;o["age"]=p.age;o["weight"]=p.weight;o["gender"]=p.gender;o["hr_max_mode"]=p.hr_max_mode;o["hr_max_manual"]=p.hr_max_manual;o["resting_heart_rate"]=p.resting_heart_rate;o["hr_zone_method"]=p.hr_zone_method;}});
}
inline std::string status(){
 auto body=encode(items,active);
 body.pop_back();
 return body+",\"generation\":"+std::to_string(generation)+",\"revision\":"+std::to_string(revision)+",\"applied_revision\":"+std::to_string(applied_revision)+",\"pending\":"+(pending_apply?"true":"false")+",\"locked\":"+((gate_locked||treadmill_recorder::busy())?"true":"false")+",\"error\":\""+error+"\"}";
}
// FsLease required. Stored applied settings, not a live computed HR sensor.
inline std::string context(){
 return esphome::json::build_json([&](JsonObject o){
  o["ready"]=ready;o["pending"]=pending_apply;o["generation"]=generation;o["applied_revision"]=applied_revision;
  const Profile *p=current();if(!ready||!p){o["profile"]=nullptr;return;}
  JsonObject q=o["profile"].to<JsonObject>();q["id"]=p->id;q["name"]=p->name;q["age"]=p->age;q["weight_kg"]=p->weight;q["sex"]=p->gender;q["hr_max_mode"]=p->hr_max_mode;q["hr_max_manual"]=p->hr_max_manual;q["resting_heart_rate"]=p->resting_heart_rate;q["hr_zone_method"]=p->hr_zone_method;
 });
}
inline bool expected_owner(const std::string &owner){return !owner.empty()&&owner==library_owner();}
inline bool decode(const std::string&body,std::vector<Profile>&list,std::string&selected){
 if(body.empty()||body.size()>8192)return false;
 return esphome::json::parse_json(body,[&](JsonObject root)->bool{
  if(root.size()!=3||!root["version"].is<int>()||root["version"].as<int>()!=1||!root["active"].is<const char*>()||!root["items"].is<JsonArray>())return false;
  selected=root["active"].as<std::string>();list.clear();JsonArray arr=root["items"];if(arr.size()>8)return false;
  for(JsonVariant v:arr){if(!v.is<JsonObject>())return false;JsonObject o=v.as<JsonObject>();if(o.size()!=9)return false;
   for(const char*k:{"id","name","gender","hr_max_mode","hr_zone_method"})if(!o[k].is<const char*>())return false;
   for(const char*k:{"age","weight","hr_max_manual","resting_heart_rate"})if(!o[k].is<int>())return false;
   Profile p;p.id=o["id"].as<std::string>();p.name=o["name"].as<std::string>();p.gender=o["gender"].as<std::string>();p.hr_max_mode=o["hr_max_mode"].as<std::string>();p.hr_zone_method=o["hr_zone_method"].as<std::string>();p.age=o["age"].as<int>();p.weight=o["weight"].as<int>();p.hr_max_manual=o["hr_max_manual"].as<int>();p.resting_heart_rate=o["resting_heart_rate"].as<int>();list.push_back(p);
  }return valid_store(list,selected);
 });
}
inline bool save(const std::vector<Profile>&list,const std::string&selected){
 const std::string body=encode(list,selected),dest=path(),tmp=dest+".tmp";if(body.empty()||body.size()>8192)return false;
 FILE*f=fopen(tmp.c_str(),"wb");if(!f)return false;
 bool ok=fwrite(body.data(),1,body.size(),f)==body.size();if(fflush(f)!=0)ok=false;if(fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;
 if(!ok||rename(tmp.c_str(),dest.c_str())!=0){unlink(tmp.c_str());return false;}return true;
}
// Main-loop only: capture initial restored values; never replace an unreadable existing file.
// HA edits are coalesced: require 3 identical observations, then one atomic file replacement.
inline void tick(){
 if(!capture||!apply||!locked)return;
 treadmill_recorder::FsLease lease;if(!lease.owned)return;
 gate_locked=locked();
 if(gate_locked||treadmill_recorder::busy()||!treadmill_recorder::initialized||!treadmill_storage::mounted)return;
 if(pending_apply){
  if(!save(queued,queued_active)){error="write_failed";pending_apply=false;queued.clear();return;}
  items=std::move(queued);active=queued_active;apply(*current());snapshot_owner(*current());++revision;applied_revision=revision;pending_apply=false;error.clear();return;
 }
 if(!ready){
  struct stat st{};if(stat(path().c_str(),&st)!=0){if(errno!=ENOENT){error="stat_failed";return;}Profile p=capture();p.id="p1";p.name="Пользователь";if(!valid(p))return;std::vector<Profile> seed{p};if(!save(seed,p.id)){error="write_failed";return;}items=seed;active=p.id;
  }else{
   if(st.st_size<1||st.st_size>8192){error="file_size";return;}FILE*f=fopen(path().c_str(),"rb");if(!f){error="read_failed";return;}std::string body(st.st_size,'\0');bool ok=fread(body.data(),1,body.size(),f)==body.size();if(fclose(f)!=0)ok=false;
   if(!ok||!decode(body,items,active)){error="invalid_file";return;}apply(*current());
  }snapshot_owner(*current());ready=true;error.clear();return;
 }
 Profile now=capture(),*p=current();if(!p)return;now.id=p->id;now.name=p->name;
 static Profile pending;static unsigned stable=0;
 if(same_settings(now,*p)){stable=0;return;}if(!valid(now)){stable=0;return;}
 if(!same_settings(now,pending)){pending=now;stable=1;return;}if(++stable<3)return;
 auto next=items;for(auto &q:next)if(q.id==active)q=now;if(save(next,active)){items=std::move(next);++revision;applied_revision=revision;error.clear();stable=0;}else error="write_failed";
}
class Handler:public AsyncWebHandler {
 std::mutex body_mutex_;
 std::string body_;httpd_req_t*request_=nullptr;bool valid_=false;
 void reply(AsyncWebServerRequest*r,int code,const std::string&body){auto*res=r->beginResponse(code,"application/json",body);res->addHeader("Cache-Control","no-store");r->send(res);}
 public:
 bool canHandle(AsyncWebServerRequest*r)const override{char b[AsyncWebServerRequest::URL_BUF_SIZE];auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b,sizeof(b)));return u=="/api/profiles"&&(r->method()==HTTP_GET||r->method()==HTTP_POST);}
 void handleBody(AsyncWebServerRequest*r,uint8_t*d,size_t n,size_t i,size_t total)override{std::lock_guard<std::mutex> guard(body_mutex_);if(i==0){body_.clear();request_=*r;valid_=total>0&&total<=8192;}if(!valid_||request_!=static_cast<httpd_req_t*>(*r)||i!=body_.size()||i>total||n>total-i){valid_=false;return;}body_.append(reinterpret_cast<char*>(d),n);}
 void handleRequest(AsyncWebServerRequest*r)override{
  std::string body;bool body_ok=false;
  if(r->method()==HTTP_POST){std::lock_guard<std::mutex> guard(body_mutex_);if(request_==static_cast<httpd_req_t*>(*r)){body=std::move(body_);body_.clear();body_ok=valid_&&body.size()==r->contentLength();valid_=false;}}

  treadmill_recorder::FsLease lease;if(!lease.owned){reply(r,409,"{\"error\":\"busy\"}");return;}if(!ready){reply(r,503,"{\"error\":\"profiles_not_ready\"}");return;}
  if(r->method()==HTTP_GET){reply(r,200,status());return;}
  if(pending_apply||gate_locked||treadmill_recorder::busy()){reply(r,409,"{\"error\":\"workout_active_or_service_busy\"}");return;}
  auto ct=r->get_header("Content-Type");std::vector<Profile> next;std::string selected;
  uint32_t expected=0,expected_generation=0;std::string payload;
  bool envelope=esphome::json::parse_json(body,[&](JsonObject root)->bool{if(!root["generation"].is<uint32_t>()||!root["revision"].is<uint32_t>())return false;expected=root["revision"].as<uint32_t>();expected_generation=root["generation"].as<uint32_t>();root.remove("generation");root.remove("revision");payload=esphome::json::build_json([&](JsonObject out){out.set(root);});return true;});
  if(!body_ok||!ct.has_value()||ct.value()!="application/json"||!envelope||!decode(payload,next,selected)){reply(r,400,"{\"error\":\"invalid_profiles\"}");return;}
  if(expected_generation!=generation||expected!=revision){reply(r,409,"{\"error\":\"revision_conflict\"}");return;}
  // Queue under FsLease; only the main loop persists and touches component state.
  queued=std::move(next);queued_active=selected;pending_apply=true;error.clear();
  reply(r,202,"{\"generation\":"+std::to_string(generation)+",\"pending\":true,\"target_revision\":"+std::to_string(revision+1)+"}");
 }
};
inline Handler handler;
}
