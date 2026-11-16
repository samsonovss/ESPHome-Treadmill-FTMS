// Host adapter harness: real ArduinoJson, fake ESPHome/HTTP and recorder lease.
#include <ArduinoJson.h>
#include <span>
#include <optional>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <fstream>
#include <thread>
#include <atomic>
namespace esphome::json {
template<class F> std::string build_json(F f){JsonDocument d;f(d.to<JsonObject>());std::string s;serializeJson(d,s);return s;}
template<class F> bool parse_json(const std::string&s,F f){JsonDocument d;if(deserializeJson(d,s))return false;return f(d.as<JsonObject>());}
}
namespace treadmill_storage {inline const char* BASE_PATH="/tmp/oc-profile-store";inline bool mounted=true;}
namespace treadmill_recorder {inline std::mutex mutex;inline bool initialized=true;inline bool busy(){return false;}struct FsLease{bool owned;FsLease():owned(mutex.try_lock()){}~FsLease(){if(owned)mutex.unlock();}};}
struct httpd_req_t{};
enum {HTTP_GET,HTTP_POST};
struct Response{void addHeader(const char*,const char*){}};
struct AsyncWebServerRequest{static constexpr size_t URL_BUF_SIZE=128;httpd_req_t raw;int verb=HTTP_POST,code=0;std::string body,result;operator httpd_req_t*(){return &raw;}std::string url_to(std::span<char,URL_BUF_SIZE>){return "/api/profiles";}int method(){return verb;}size_t contentLength(){return body.size();}std::optional<std::string>get_header(const char*){return "application/json";}Response*beginResponse(int c,const char*,const std::string&s){code=c;result=s;static Response r;return &r;}void send(Response*){}};
struct AsyncWebHandler{virtual bool canHandle(AsyncWebServerRequest*)const{return false;}virtual void handleBody(AsyncWebServerRequest*,uint8_t*,size_t,size_t,size_t){}virtual void handleRequest(AsyncWebServerRequest*){}};
#include "../esphome/treadmill_profiles.h"
using namespace treadmill_profiles;
void submit(AsyncWebServerRequest&r){handler.handleBody(&r,reinterpret_cast<uint8_t*>(r.body.data()),r.body.size(),0,r.body.size());handler.handleRequest(&r);}
std::string envelope(){auto s=encode(items,active);s.pop_back();return s+",\"generation\":"+std::to_string(generation)+",\"revision\":"+std::to_string(revision)+"}";}
int main(){mkdir(treadmill_storage::BASE_PATH,0700);unlink(path().c_str());Profile p;p.id="p1";p.name="User";capture=[&]{return p;};apply=[&](const Profile&q){p=q;};bool blocked=false;locked=[&]{return blocked;};tick();assert(ready);std::vector<Profile>v;std::string a;assert(decode(encode(items,active),v,a));assert(!decode("{}",v,a));
AsyncWebServerRequest r;r.body=envelope();submit(r);assert(r.code==202&&pending_apply);blocked=true;tick();assert(pending_apply&&revision==1);blocked=false;tick();assert(!pending_apply&&revision==2);
submit(r);assert(r.code==409);r.body=envelope();submit(r);assert(r.code==202);rename(treadmill_storage::BASE_PATH,"/tmp/oc-profile-store-held");tick();assert(error=="write_failed"&&revision==2&&!pending_apply);rename("/tmp/oc-profile-store-held",treadmill_storage::BASE_PATH);
// Overlapping requests must never apply mixed bytes or overwrite a queued CAS transaction.
AsyncWebServerRequest x,y;x.body=envelope();y.body=x.body;size_t h=x.body.size()/2;handler.handleBody(&x,(uint8_t*)x.body.data(),h,0,x.body.size());submit(y);handler.handleBody(&x,(uint8_t*)x.body.data()+h,x.body.size()-h,h,x.body.size());handler.handleRequest(&x);assert(y.code==202&&x.code!=202);tick();assert(revision==3);
AsyncWebServerRequest z;z.body=envelope();h=z.body.size()/2;handler.handleBody(&z,(uint8_t*)z.body.data(),h,0,z.body.size());AsyncWebServerRequest g;g.verb=HTTP_GET;handler.handleRequest(&g);assert(g.code==200);handler.handleBody(&z,(uint8_t*)z.body.data()+h,z.body.size()-h,h,z.body.size());handler.handleRequest(&z);assert(z.code==202);tick();assert(revision==4);
std::atomic<int>reads{0};std::thread t([&]{for(int i=0;i<1000;i++){AsyncWebServerRequest g;g.verb=HTTP_GET;handler.handleRequest(&g);assert(g.code==200||g.code==409);reads++;}});for(int i=0;i<1000;i++)tick();t.join();assert(reads==1000);puts("PROFILE_ADAPTER_PASS");}
