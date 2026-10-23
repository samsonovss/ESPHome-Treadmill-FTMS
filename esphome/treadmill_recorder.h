#pragma once
#include "recorder_core.h"
#include "treadmill_storage.h"
#include "gzlite.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "freertos/queue.h"
#include "esphome/core/log.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include "esp_heap_caps.h"
namespace treadmill_recorder {
inline Engine engine(treadmill_storage::BASE_PATH);
struct Event { uint8_t kind; Snapshot s; char id[12]{}; }; // start=1 sample=2 finish=3
inline QueueHandle_t queue=nullptr;
inline std::atomic<bool> control_busy{false},recording{false},io_busy{true},healthy{false},initialized{false};
inline std::atomic<uint32_t> latest_id{0};
inline std::atomic<int> min_save_sec{0};   // workouts shorter than this (workout time, s) are deleted at stop; 0 = keep all (12.10)
inline std::atomic<int> fault{0};
inline std::atomic<size_t> archive_total{0},archive_used{0};
inline void capacity(){size_t t=0,u=0;if(esp_littlefs_info(treadmill_storage::PARTITION,&t,&u)==ESP_OK){archive_total=t;archive_used=u;}}
inline std::atomic_flag fs_lock=ATOMIC_FLAG_INIT;
struct FsLease { bool owned; FsLease():owned(!fs_lock.test_and_set()){} ~FsLease(){if(owned)fs_lock.clear();} };
inline std::atomic<bool> mutation_pending{false};
inline std::atomic<uint32_t> job_number{0};
inline std::atomic<int> job_state{0}; // 1 queued 2 running 3 succeeded 4 failed
inline std::string job_id,job_operation,job_error; // always protected by fs_lock
inline uint32_t queued_sample_ms=0;
inline const char *fault_text(){switch(fault.load()){case 1:return "storage_unavailable";case 2:return "io_error_or_full";case 3:return "queue_overflow";case 4:return "worker_unavailable";case 5:return "storage_full";default:return "";}}
inline bool busy(); // Used by the program HTTP handler defined below.
// ---- 17.10: finished workouts are gzip-compressed in the background (worker task, PSRAM buffers, never while running).
// <id>.csv → <id>.csvz.tmp (fsync) → rename <id>.csvz → unlink <id>.csv. A power cut at any step leaves a readable copy;
// the next pass finishes the job. Served to the browser with Content-Encoding: gzip (decompressed transparently).
inline std::atomic<bool> compress_pending{true};
inline void *ps_alloc(size_t n){void *p=heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);return p;}
inline bool compress_file(const std::string &id){
  auto csv=engine.path(id,"csv"),gz=engine.path(id,"csvz"),tmp=engine.path(id,"csvz.tmp");struct stat st{};
  if(stat(csv.c_str(),&st)!=0||st.st_size<=0||st.st_size>4*1024*1024)return false;
  const size_t n=size_t(st.st_size),cap=n+n/8+1024;
  std::unique_ptr<uint8_t,void(*)(void*)> in(static_cast<uint8_t*>(ps_alloc(n)),heap_caps_free),out(static_cast<uint8_t*>(ps_alloc(cap)),heap_caps_free);
  std::unique_ptr<uint32_t,void(*)(void*)> work(static_cast<uint32_t*>(ps_alloc((65536+32768)*4)),heap_caps_free);
  if(!in||!out||!work)return false;
  FILE *f=fopen(csv.c_str(),"rb");if(!f)return false;size_t got=fread(in.get(),1,n,f);fclose(f);if(got!=n)return false;
  const size_t z=gzlite::gzip(in.get(),n,out.get(),cap,work.get(),[]{vTaskDelay(1);});if(!z)return false;
  int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0)return false;
  bool ok=true;for(size_t w=0;w<z;){ssize_t k=write(fd,out.get()+w,std::min<size_t>(4096,z-w));if(k<=0){ok=false;break;}w+=size_t(k);vTaskDelay(1);}
  if(fsync(fd)!=0)ok=false;if(close(fd)!=0)ok=false;
  if(!ok||rename(tmp.c_str(),gz.c_str())!=0){unlink(tmp.c_str());return false;}
  unlink(csv.c_str());ESP_LOGI("recorder","Workout %s compressed %u -> %u bytes",id.c_str(),unsigned(n),unsigned(z));return true;
}
// One step per idle tick: tidy leftovers, compress the oldest finished workout. false = nothing left to do.
inline bool compress_step(){
  const uint64_t max=latest_id.load();
  for(uint64_t k=1;k<=max;k++){char n[24];snprintf(n,sizeof n,"w%010llu",(unsigned long long)k);if((k&15)==0)vTaskDelay(1);
    struct stat st{};std::string csv=engine.path(n,"csv"),gz=engine.path(n,"csvz"),js=engine.path(n,"json"),tmp=engine.path(n,"csvz.tmp");
    if(stat(tmp.c_str(),&st)==0)unlink(tmp.c_str());
    const bool hc=stat(csv.c_str(),&st)==0;if(!hc)continue;
    if(stat(gz.c_str(),&st)==0){unlink(csv.c_str());continue;}   // renamed but csv not yet removed
    if(stat(js.c_str(),&st)!=0)continue;                          // not finished (or being recovered)
    if(engine.active&&engine.id==n)continue;
    compress_file(n);return true;}
  return false;
}
// ---- Storage inventory (17.10): run by the worker task, never by HTTP (a long walk in httpd tripped the task WDT).
// Pauses after every entry; result is a small JSON kept until the next scan. Read-only: nothing is written.
inline std::atomic<int> scan_state{0}; // 0 none, 1 queued, 2 running, 3 ready, 4 failed
inline std::string scan_out;           // guarded by fs_lock
inline bool scan_run(){
  struct Big{std::string p;long long s;};std::vector<Big> big;std::vector<std::string> todo{treadmill_storage::BASE_PATH};std::string dirs;long long nf=0,total=0;int ndir=0;
  while(!todo.empty()&&ndir<64){std::string d=todo.back();todo.pop_back();DIR *dp=opendir(d.c_str());if(!dp)continue;ndir++;long long cnt=0,bytes=0;
    while(struct dirent *e=readdir(dp)){vTaskDelay(pdMS_TO_TICKS(2));
      if(e->d_name[0]=='.'&&(e->d_name[1]==0||(e->d_name[1]=='.'&&e->d_name[2]==0)))continue;std::string p=d+"/"+e->d_name;struct stat st;if(stat(p.c_str(),&st)!=0)continue;
      if(S_ISDIR(st.st_mode)){todo.push_back(p);continue;}cnt++;bytes+=st.st_size;nf++;total+=st.st_size;
      big.push_back({p.substr(strlen(treadmill_storage::BASE_PATH)),(long long)st.st_size});if(big.size()>40){std::sort(big.begin(),big.end(),[](const Big&a,const Big&b){return a.s>b.s;});big.resize(20);}}
    closedir(dp);if(!dirs.empty())dirs+=',';dirs+="{\"dir\":"+json_string(d.c_str())+",\"files\":"+std::to_string(cnt)+",\"bytes\":"+std::to_string(bytes)+"}";}
  std::sort(big.begin(),big.end(),[](const Big&a,const Big&b){return a.s>b.s;});if(big.size()>20)big.resize(20);
  std::string bl;for(auto &b:big){if(!bl.empty())bl+=',';bl+="["+json_string(b.p.c_str())+","+std::to_string(b.s)+"]";}
  capacity();scan_out="{\"state\":\"ready\",\"used_bytes\":"+std::to_string(archive_used.load())+",\"total_bytes\":"+std::to_string(archive_total.load())+",\"file_count\":"+std::to_string(nf)+",\"file_bytes\":"+std::to_string(total)+",\"dirs\":["+dirs+"],\"largest\":["+bl+"]}";
  return true;
}
// ---- Live workout buffer (PSRAM). Observation copy of rows already written to the CSV;
// lets the web UI draw the whole current workout even if the page was opened mid-run.
// Never read by control logic; PSRAM falls back to normal heap if unavailable.
template<typename T> struct PsramAllocator {
  using value_type=T; PsramAllocator() noexcept=default; template<typename U> PsramAllocator(const PsramAllocator<U>&) noexcept {}
  T *allocate(std::size_t n){void *m=heap_caps_malloc(n*sizeof(T),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!m)m=heap_caps_malloc(n*sizeof(T),MALLOC_CAP_8BIT);if(!m)std::abort();return static_cast<T*>(m);}
  void deallocate(T *p,std::size_t) noexcept {heap_caps_free(p);}
  template<typename U> bool operator==(const PsramAllocator<U>&) const noexcept {return true;}
  template<typename U> bool operator!=(const PsramAllocator<U>&) const noexcept {return false;}
};
struct LiveRow { float elapsed,timer,hr,speed,level,distance,calories; uint8_t paused,zone,target_zone,pad; };
constexpr size_t LIVE_MAX_ROWS=18000; // 5 h at 1 Hz, ~576 KB in PSRAM
inline std::mutex live_mutex; // guards everything below
inline std::vector<LiveRow,PsramAllocator<LiveRow>> live_rows;
inline std::string live_id; inline char live_program[64]{}; inline int64_t live_start_epoch=0; inline bool live_active=false;
inline void live_push(const Snapshot &s){
  LiveRow r{float(engine.elapsed),float(engine.timer),std::isfinite(s.hr)&&s.hr>0?s.hr:NAN,s.speed,s.level,float(engine.distance),engine.cal,uint8_t(s.paused),uint8_t(std::max(0,std::min(255,s.zone))),uint8_t(std::max(0,std::min(255,s.target_zone))),0};
  std::lock_guard<std::mutex> g(live_mutex);if(live_rows.size()<LIVE_MAX_ROWS)live_rows.push_back(r);
}
} // namespace treadmill_recorder
#include "treadmill_programs.h"
#include "treadmill_presets.h"
namespace treadmill_recorder {
inline void worker(void *) {
  if(!treadmill_storage::mounted||!engine.initialize()){fault=1;io_busy=false;vTaskDelete(nullptr);return;}
  latest_id=uint32_t(std::min(engine.sequence,uint64_t(UINT32_MAX)));initialized=true;healthy=engine.error.empty();if(!healthy)fault=engine.error=="storage_full"?5:2;capacity();io_busy=false;
  Event e{};for(;;){if(xQueueReceive(queue,&e,pdMS_TO_TICKS(1000))!=pdTRUE){
      if(!healthy&&engine.active){while(fs_lock.test_and_set())vTaskDelay(pdMS_TO_TICKS(1));engine.finish(engine.previous,true);io_busy=false;recording=false;fs_lock.clear();}
      else if(compress_pending&&!recording&&!engine.active&&!control_busy&&!mutation_pending&&uxQueueMessagesWaiting(queue)==0&&!fs_lock.test_and_set()){
        io_busy=true;if(!compress_step())compress_pending=false;capacity();io_busy=false;fs_lock.clear();}
      continue;
    }io_busy=true;
    if(e.kind>=4){
      // HTTP never performs moves. Reserve worker ownership against downloads.
      while(fs_lock.test_and_set())vTaskDelay(pdMS_TO_TICKS(1));
      job_state=2;bool ok=false;
      if(control_busy||recording||engine.active||uxQueueMessagesWaiting(queue)>0)job_error="workout_active_or_finalizing";
      else if(e.kind==6){ok=plan_write();plan_payload.clear();}
      else if(e.kind==7){ok=engine.purge(e.id);job_error=ok?"":engine.error;}
      else if(e.kind==8){scan_state=2;ok=scan_run();scan_state=ok?3:4;}
      else {ok=engine.move_selected(e.id,e.kind==5);job_error=ok?"":engine.error;}
      job_state=ok?3:4;capacity();mutation_pending=false;compress_pending=true;
      io_busy=engine.active||uxQueueMessagesWaiting(queue)>0;fs_lock.clear();continue;
    }
    while(fs_lock.test_and_set())vTaskDelay(pdMS_TO_TICKS(1));
    const unsigned before=engine.samples;const bool was_active=engine.active;
    bool ok=e.kind==1?engine.start(e.s):(e.kind==3?engine.finish(e.s,fault.load()==3):(!engine.active||engine.sample(e.s)));
    if(ok&&e.kind==1&&engine.active&&!was_active){std::lock_guard<std::mutex> g(live_mutex);live_rows.clear();live_rows.reserve(3600);live_id=engine.id;memcpy(live_program,engine.program,sizeof live_program);live_start_epoch=engine.start_epoch;live_active=true;}
    if(ok&&engine.samples!=before)live_push(e.s);
    if(e.kind==3||!engine.active){std::lock_guard<std::mutex> g(live_mutex);live_active=false;}
    if(e.kind==3)compress_pending=true;
    if(ok&&e.kind==3&&!engine.active){int lim=min_save_sec.load();std::string fid=engine.id;
      if(lim>0&&std::isfinite(engine.timer)&&engine.timer<lim){
        bool gone=engine.purge(fid);
        ESP_LOGI("recorder","Workout %s: %.0f s < %d s -> %s",fid.c_str(),engine.timer,lim,gone?"not saved":"delete failed");}}
    latest_id=uint32_t(std::min(engine.sequence,uint64_t(UINT32_MAX)));if(e.kind==3||!ok)capacity();if(!ok){fault=engine.error=="storage_full"?5:2;healthy=false;recording=false;}io_busy=engine.active||uxQueueMessagesWaiting(queue)>0;fs_lock.clear();
  }
}
inline void initialize(){if(queue)return;queue=xQueueCreate(32,sizeof(Event));if(!queue){fault=4;io_busy=false;return;}if(xTaskCreate(worker,"workout_io",6144,nullptr,1,nullptr)!=pdPASS){fault=4;io_busy=false;}}
inline void enqueue(uint8_t kind,const Snapshot &s){if(!queue||(!healthy&&kind!=3)){if(!fault.load())fault=4;return;}Event e{kind,s};if(xQueueSend(queue,&e,0)!=pdTRUE){fault=3;healthy=false;if(kind==1)recording=false;}}
inline void start(const Snapshot &s){if(!healthy)return;if(recording.exchange(true))return;io_busy=true;queued_sample_ms=s.ms;enqueue(1,s);}
inline void sample(const Snapshot &s){if(!recording||!healthy)return;if(uint32_t(s.ms-queued_sample_ms)<1000)return;queued_sample_ms=s.ms;
  // Four slots reserved for lifecycle events; overflow fails visibly, never controls motor.
  if(uxQueueSpacesAvailable(queue)<=4){fault=3;healthy=false;return;}enqueue(2,s);
}
inline void transition(const Snapshot &s){if(recording)enqueue(2,s);}
inline void finish(const Snapshot &s){if(!recording.exchange(false))return;enqueue(3,s);}
inline bool busy(){return recording||io_busy||control_busy||mutation_pending;}
class Handler: public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *r) const override {
    std::unique_ptr<char[]> b(new (std::nothrow) char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!b)return false;
    auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE));
    return (r->method()==HTTP_GET&&(u=="/workouts/status"||u=="/workouts/list"||u=="/workouts/file"||u=="/api/workouts/job"||u=="/api/workouts/trash/list"||u=="/api/workouts/live"||u=="/api/storage/scan")) ||
      (r->method()==HTTP_POST&&(u=="/api/workouts/trash"||u=="/api/workouts/restore"||u=="/api/workouts/purge"||u=="/api/storage/scan"));
  }
  void send(AsyncWebServerRequest *r,int code,const std::string &s){auto *res=r->beginResponse(code,"application/json",s);res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);}
  std::string param(AsyncWebServerRequest *r,const char *name){auto *p=r->getParam(name);return p?p->value().c_str():"";}
  void handleRequest(AsyncWebServerRequest *r) override {
    // IDF httpd has only 4352 bytes of stack in ESPHome 2026.9.1.
    // Keep URI and file buffers on the heap: a function's file-branch array
    // consumes stack even when handling /status before any archive exists.
    std::unique_ptr<char[]> b(new (std::nothrow) char[AsyncWebServerRequest::URL_BUF_SIZE]);
    if(!b){send(r,503,"{\"error\":\"response_memory_unavailable\"}");return;}
    auto u=r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE));
    if(u=="/api/storage/scan"){
      if(r->method()==HTTP_POST){
        if(busy()){send(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}
        if(!initialized||!treadmill_storage::mounted||!queue){send(r,503,"{\"error\":\"storage_not_ready_or_error\"}");return;}
        Event e{};e.kind=8;mutation_pending=true;scan_state=1;job_id="";job_operation="scan";job_error.clear();job_state=1;
        if(xQueueSend(queue,&e,0)!=pdTRUE){mutation_pending=false;job_state=0;scan_state=0;send(r,503,"{\"error\":\"queue_unavailable\"}");return;}
        ++job_number;if(!job_number)++job_number;send(r,202,"{\"state\":\"queued\"}");return;}
      const int st=scan_state.load();
      if(st!=3){send(r,200,std::string("{\"state\":")+(st==1?"\"queued\"":st==2?"\"running\"":st==4?"\"failed\"":"\"none\"")+"}");return;}
      FsLease lease;if(!lease.owned){send(r,200,"{\"state\":\"running\"}");return;}
      std::string out=scan_out;send(r,200,out);return;
    }
    if(u=="/workouts/status") {
      std::string out;out.reserve(256);
      out="{\"state\":";out+=json_string(recording?"recording":(io_busy?"finalizing":"idle"));
      out+=",\"recording\":";out+=busy()?"true":"false";
      out+=",\"archive_available\":";out+=initialized?"true":"false";
      out+=",\"error\":";out+=json_string(fault_text());
      out+=",\"total_bytes\":";out+=std::to_string(archive_total.load());
      out+=",\"used_bytes\":";out+=std::to_string(archive_used.load());
      out+='}';send(r,200,out);return;
    }
    if(u=="/api/workouts/live"){
      // Read-only, no filesystem: allowed while recording. Paged, max 1200 rows per response.
      auto f=param(r,"from");size_t from=0;for(char c:f){if(c<'0'||c>'9'){send(r,400,"{\"error\":\"invalid_from\"}");return;}from=from*10+size_t(c-'0');if(from>LIVE_MAX_ROWS)break;}
      std::string out;
      {std::lock_guard<std::mutex> g(live_mutex);const size_t n=live_rows.size(),end=std::min(n,from+1200);
       out.reserve(160+(end>from?end-from:0)*72);
       out="{\"id\":";out+=live_id.empty()?"null":json_string(live_id.c_str());out+=",\"active\":";out+=live_active?"true":"false";
       out+=",\"program\":";out+=json_string(live_program);out+=",\"start_epoch\":";out+=std::to_string((long long)live_start_epoch);
       out+=",\"total\":";out+=std::to_string(n);out+=",\"from\":";out+=std::to_string(from);out+=",\"columns\":[\"elapsed_s\",\"time_s\",\"heart_rate\",\"speed_kmh\",\"incline_level\",\"distance_km\",\"calories\",\"paused\",\"zone\",\"target_zone\"],\"rows\":[";
       char line[160];auto num=[](char *b,size_t sz,float v,const char *fmt){if(std::isfinite(v))snprintf(b,sz,fmt,v);else snprintf(b,sz,"null");};
       for(size_t i=from;i<end;i++){const auto &x=live_rows[i];char h[16],c[16];num(h,sizeof h,x.hr,"%.0f");num(c,sizeof c,x.calories,"%.1f");
         snprintf(line,sizeof line,"%s[%.1f,%.1f,%s,%.2f,%.1f,%.4f,%s,%u,%u,%u]",i>from?",":"",x.elapsed,x.timer,h,x.speed,x.level,x.distance,c,x.paused,x.zone,x.target_zone);out+=line;}
       out+="]}";}
      auto *res=r->beginResponse(200,"application/json",reinterpret_cast<const uint8_t*>(out.data()),out.size());res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);return;
    }
    FsLease lease;
    if(!lease.owned){send(r,409,"{\"error\":\"archive_busy\"}");return;}
    if(u=="/api/workouts/job"){
      auto j=param(r,"job");bool digits=!j.empty()&&j.size()<=10;for(char c:j)digits=digits&&c>='0'&&c<='9';
      if(!digits){send(r,400,"{\"error\":\"invalid_job\"}");return;}
      if(!job_number||strtoull(j.c_str(),nullptr,10)!=job_number){send(r,404,"{\"error\":\"unknown_job\"}");return;}
      const char *states[]={"unknown","queued","running","succeeded","failed"};
      send(r,200,"{\"job\":"+std::to_string(job_number.load())+",\"state\":"+json_string(states[job_state.load()])+",\"operation\":"+json_string(job_operation.c_str())+",\"id\":"+json_string(job_id.c_str())+",\"error\":"+json_string(job_error.c_str())+"}");return;
    }
    if(u=="/api/workouts/trash"||u=="/api/workouts/restore"||u=="/api/workouts/purge"){
      if(r->method()!=HTTP_POST){send(r,405,"{\"error\":\"method_not_allowed\"}");return;}
      httpd_req_t *raw=*r;
      const char *query=strchr(raw->uri,'?');
      // Raw exact grammar rejects duplicate IDs, extra fields, body overrides,
      // encoded separators and ambiguity before ESPHome's first-value lookup.
      if(r->contentLength()!=0||!query||strncmp(query+1,"id=",3)!=0||strlen(query+4)!=11){send(r,400,"{\"error\":\"invalid_id\"}");return;}
      std::string id(query+4);if(!valid_id(id)){send(r,400,"{\"error\":\"invalid_id\"}");return;}
      if(busy()){send(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}
      // Recording can be disabled by ENOSPC while stopped archives remain readable
      // and movable. Do not require recorder health for a reversible archive move.
      if(!initialized||!treadmill_storage::mounted||!queue){send(r,503,"{\"error\":\"storage_not_ready_or_error\"}");return;}
      Event e{};e.kind=u=="/api/workouts/trash"?4:(u=="/api/workouts/purge"?7:5);memcpy(e.id,id.c_str(),12);
      mutation_pending=true;job_id=id;job_operation=e.kind==4?"trash":(e.kind==7?"purge":"restore");job_error.clear();job_state=1;
      if(xQueueSend(queue,&e,0)!=pdTRUE){mutation_pending=false;job_state=0;send(r,503,"{\"error\":\"queue_unavailable\"}");return;}
      ++job_number;if(!job_number)++job_number;
      send(r,202,"{\"job\":"+std::to_string(job_number.load())+",\"state\":\"queued\",\"operation\":"+json_string(job_operation.c_str())+",\"id\":"+json_string(id.c_str())+"}");return;
    }
    if(busy()){send(r,409,"{\"error\":\"workout_active_or_finalizing\"}");return;}
    if(!initialized){send(r,503,"{\"error\":\"storage_not_ready_or_error\"}");return;}
    if(u=="/workouts/list"||u=="/api/workouts/trash/list") {
      bool trashed=u=="/api/workouts/trash/list";
      auto after=param(r,"after");if(!after.empty()&&!valid_id(after)){send(r,400,"{\"error\":\"invalid_cursor\"}");return;}
      auto l=param(r,"limit");int limit=l.empty()?20:atoi(l.c_str());if(limit<1||limit>20){send(r,400,"{\"error\":\"invalid_limit\"}");return;}
      const bool with_summary=!trashed&&param(r,"summaries")=="1";
      uint64_t cursor=after.empty()?0:strtoull(after.substr(1).c_str(),nullptr,10),max=latest_id.load();std::string out="{\"items\":[";unsigned scanned=0,count=0;
      // At most 100 IDs / 200 stat calls per page, including gaps; no entire-directory JSON.
      while(cursor<max&&scanned++<100&&count<unsigned(limit)){
        cursor++;char n[24];snprintf(n,sizeof n,"w%010llu",(unsigned long long)cursor);struct stat st{};
        char tst=engine.trash_state(n);bool tomb=tst!='0'&&tst!='D';if(trashed){if(tst=='T'){ /* purged ('P') = gone, not in the trash */if(count++)out+=',';out+="{\"id\":"+json_string(n)+"}";}continue;}if(tomb)continue;
        bool complete=stat(engine.path(n,"json").c_str(),&st)==0;
        if(!complete&&errno!=ENOENT){send(r,500,"{\"error\":\"index_read_failed\"}");return;}
        bool exists=complete||stat(engine.path(n,"csv").c_str(),&st)==0||stat(engine.path(n,"csvz").c_str(),&st)==0;
        if(!exists&&errno!=ENOENT){send(r,500,"{\"error\":\"index_read_failed\"}");return;}
        if(exists){if(count++)out+=',';out+="{\"id\":"+json_string(n)+",\"summary_available\":"+(complete?"true":"false");
          if(with_summary&&complete&&st.st_size>0&&st.st_size<=2048){FILE *sf=fopen(engine.path(n,"json").c_str(),"rb");if(sf){std::string js(size_t(st.st_size),'\0');size_t got=fread(js.data(),1,js.size(),sf);fclose(sf);if(got==js.size()&&js.front()=='{'&&js.back()=='}'){out+=",\"summary\":";out+=js;}}}
          out+="}";}
      }
      out+="],\"next\":";if(cursor<max){char n[24];snprintf(n,sizeof n,"w%010llu",(unsigned long long)cursor);out+=json_string(n);}else out+="null";send(r,200,out+"}");return;
    }
    auto id=param(r,"id"),type=param(r,"type");if(!valid_id(id)||(type!="csv"&&type!="json")){send(r,400,"{\"error\":\"invalid_file\"}");return;}
    if(engine.tombstoned(id)){send(r,404,"{\"error\":\"workout_in_trash\"}");return;}
    // 16 KB chunks from PSRAM: far fewer httpd round trips than 1 KB; internal 1 KB fallback.
    size_t CHUNK_SIZE=16384;
    char *buf=static_cast<char*>(heap_caps_malloc(CHUNK_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!buf){CHUNK_SIZE=1024;buf=static_cast<char*>(heap_caps_malloc(CHUNK_SIZE,MALLOC_CAP_8BIT));}
    std::unique_ptr<char,void(*)(void*)> chunk(buf,heap_caps_free);
    if(!chunk){send(r,503,"{\"error\":\"response_memory_unavailable\"}");return;}
    bool gz=false;FILE *f=nullptr;if(type=="csv"){f=fopen(engine.path(id,"csvz").c_str(),"rb");gz=f!=nullptr;}
    if(!f)f=fopen(engine.path(id,type.c_str()).c_str(),"rb");if(!f){send(r,errno==ENOENT?404:500,"{\"error\":\"file_open_failed\"}");return;}
    // ESPHome ESP-IDF does NOT provide beginChunkedResponse. Use the public
    // httpd_req_t conversion and IDF chunk API on the HTTP server task.
    auto *res=r->beginResponse(200,type=="csv"?"text/csv; charset=utf-8":"application/json");
    const std::string disposition="attachment; filename=\""+id+"."+type+"\"";
    res->addHeader("Content-Disposition",disposition.c_str());if(gz)res->addHeader("Content-Encoding","gzip");res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");
    bool ok=true;size_t n;
    while((n=fread(chunk.get(),1,CHUNK_SIZE,f))>0){if(httpd_resp_send_chunk(*r,chunk.get(),n)!=ESP_OK){ok=false;break;}}
    if(ferror(f))ok=false;
    if(fclose(f)!=0)ok=false;
    if(ok&&httpd_resp_send_chunk(*r,nullptr,0)!=ESP_OK)ok=false;
    if(!ok) { fault=2;httpd_req_t *raw=*r;httpd_sess_trigger_close(raw->handle,httpd_req_to_sockfd(raw)); }

  }
};
inline Handler handler;
inline void register_handler(){if(esphome::web_server_base::global_web_server_base)esphome::web_server_base::global_web_server_base->add_handler(&handler);if(esphome::web_server_base::global_web_server_base)esphome::web_server_base::global_web_server_base->add_handler(&program_handler);if(esphome::web_server_base::global_web_server_base)esphome::web_server_base::global_web_server_base->add_handler(&preset_handler);}
}
