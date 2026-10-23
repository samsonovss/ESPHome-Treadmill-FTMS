#pragma once
// MCP server (JSON-RPC 2.0 over HTTP POST /mcp, stateless, no SSE, no sessions).
// Tools read recorder state and summaries and can store a validated program file; nothing here can move the motor.
namespace treadmill_mcp {
using namespace treadmill_recorder;
constexpr size_t MAX_BODY=9216;

inline std::string jraw(const std::string &s,const char *key){
  const size_t n=s.size();size_t i=0;while(i<n&&s[i]!='{')i++;if(i>=n)return "";i++;int depth=0;
  auto ws=[&](size_t p){while(p<n&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))p++;return p;};
  while(i<n){
    char c=s[i];
    if(c=='"'){
      std::string k;size_t j=i+1;while(j<n&&s[j]!='"'){if(s[j]=='\\'&&j+1<n){k+=s[j+1];j+=2;}else k+=s[j++];}
      i=j+1;
      if(depth==0){size_t p=ws(i);
        if(p<n&&s[p]==':'&&k==key){p=ws(p+1);size_t q=p;int d=0;bool str=false;
          for(;q<n;q++){char x=s[q];
            if(str){if(x=='\\')q++;else if(x=='"'){str=false;if(d==0){q++;break;}}}
            else if(x=='"')str=true;
            else if(x=='{'||x=='[')d++;
            else if(x=='}'||x==']'){if(d==0)break;d--;if(d==0){q++;break;}}
            else if(x==','&&d==0)break;}
          while(q>p&&(s[q-1]==' '||s[q-1]=='\n'||s[q-1]=='\r'||s[q-1]=='\t'))q--;
          return s.substr(p,q-p);}}
      continue;
    }
    if(c=='{'||c=='[')depth++;else if(c=='}'||c==']'){if(depth==0)return "";depth--;}
    i++;
  }
  return "";
}
inline std::string jstr(const std::string &raw){if(raw.size()<2||raw.front()!='"'||raw.back()!='"')return "";std::string v=raw.substr(1,raw.size()-2);if(v.find('\\')!=std::string::npos)return "";return v;}

class McpHandler: public AsyncWebHandler {
  std::string body_;httpd_req_t *body_request_=nullptr;bool body_valid_=false;
  static std::string url(AsyncWebServerRequest *r){std::unique_ptr<char[]> b(new (std::nothrow) char[AsyncWebServerRequest::URL_BUF_SIZE]);if(!b)return "";return std::string(r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b.get(),AsyncWebServerRequest::URL_BUF_SIZE)));}
 public:
  bool canHandle(AsyncWebServerRequest *r) const override {return (r->method()==HTTP_POST||r->method()==HTTP_GET)&&url(r)=="/mcp";}
  void handleBody(AsyncWebServerRequest *r,uint8_t *data,size_t len,size_t index,size_t total) override {
    if(index==0){body_.clear();body_request_=*r;body_valid_=total>0&&total<=MAX_BODY;if(body_valid_)body_.reserve(total);}
    if(!body_valid_||body_request_!=static_cast<httpd_req_t *>(*r)||index!=body_.size()||len>total-index){body_valid_=false;return;}
    body_.append(reinterpret_cast<char *>(data),len);
  }
  void reply(AsyncWebServerRequest *r,int code,const std::string &s){auto *res=r->beginResponse(code,"application/json",s);res->addHeader("Cache-Control","no-store");res->addHeader("X-Content-Type-Options","nosniff");r->send(res);}
  static std::string ok(const std::string &id,const std::string &result){return "{\"jsonrpc\":\"2.0\",\"id\":"+id+",\"result\":"+result+"}";}
  static std::string err(const std::string &id,int code,const char *msg){return "{\"jsonrpc\":\"2.0\",\"id\":"+id+",\"error\":{\"code\":"+std::to_string(code)+",\"message\":"+json_string(msg)+"}}";}
  static std::string tool(const std::string &id,const std::string &text,bool is_error){return ok(id,"{\"content\":[{\"type\":\"text\",\"text\":"+json_string(text.c_str())+"}],\"isError\":"+(is_error?"true":"false")+"}");}

  std::string call(const std::string &id,const std::string &name,const std::string &args){
    if(name=="get_status"){
      std::string o="{\"state\":"+json_string(recording?"recording":(io_busy?"finalizing":"idle"))+",\"archive_available\":"+(initialized?"true":"false")+",\"error\":"+json_string(fault_text())+",\"total_bytes\":"+std::to_string(archive_total.load())+",\"used_bytes\":"+std::to_string(archive_used.load())+"}";
      return tool(id,o,false);
    }
    if(name=="get_live"){
      std::string o,lb(900,'\0');char *line=&lb[0];
      {std::lock_guard<std::mutex> g(live_mutex);
       if(live_rows.empty())o="{\"active\":false}";
       else{const auto &x=live_rows.back();auto num=[](float v,char *b,size_t sz,const char *fmt){if(std::isfinite(v))snprintf(b,sz,fmt,v);else snprintf(b,sz,"null");};
        char hr[16],sp[16],lv[16],di[16],ca[16];num(x.hr,hr,sizeof hr,"%.0f");num(x.speed,sp,sizeof sp,"%.1f");num(x.level,lv,sizeof lv,"%.0f");num(x.distance,di,sizeof di,"%.3f");num(x.calories,ca,sizeof ca,"%.1f");
        snprintf(line,lb.size(),"\"elapsed_s\":%.0f,\"time_s\":%.0f,\"heart_rate\":%s,\"speed_kmh\":%s,\"incline_level\":%s,\"distance_km\":%s,\"calories\":%s,\"paused\":%s,\"zone\":%u,\"target_zone\":%u",x.elapsed,x.timer,hr,sp,lv,di,ca,x.paused?"true":"false",unsigned(x.zone),unsigned(x.target_zone));
        o=std::string("{\"active\":")+(live_active?"true":"false")+",\"id\":"+(live_id.empty()?"null":json_string(live_id.c_str()))+",\"program\":"+json_string(live_program)+",\"rows\":"+std::to_string(live_rows.size())+","+line+"}";}}
      return tool(id,o,false);
    }
    if(name=="list_workouts"||name=="get_workout_summary"){
      if(busy())return tool(id,"workout_active_or_finalizing: retry after the treadmill is stopped",true);
      if(!initialized)return tool(id,"storage_not_ready",true);
      FsLease lease;if(!lease.owned)return tool(id,"archive_busy: retry in a moment",true);
      if(name=="list_workouts"){
        std::string after=jstr(jraw(args,"after")),lr=jraw(args,"limit");if(!after.empty()&&!valid_id(after))return tool(id,"invalid after",true);
        int limit=20;if(!lr.empty()){limit=atoi(lr.c_str());if(limit<1||limit>20)return tool(id,"limit must be 1..20",true);}
        uint64_t cursor=after.empty()?0:strtoull(after.substr(1).c_str(),nullptr,10),max=latest_id.load();std::string out="{\"items\":[";unsigned scanned=0,count=0;
        while(cursor<max&&scanned++<100&&count<unsigned(limit)){
          cursor++;char n[24];snprintf(n,sizeof n,"w%010llu",(unsigned long long)cursor);struct stat st{};
          if(engine.tombstoned(n))continue;
          bool complete=stat(engine.path(n,"json").c_str(),&st)==0;
          bool exists=complete||stat(engine.path(n,"csv").c_str(),&st)==0;
          if(exists){if(count++)out+=',';out+="{\"id\":"+json_string(n)+",\"summary_available\":"+(complete?"true":"false")+"}";}
        }
        out+="],\"next\":";if(cursor<max){char n[24];snprintf(n,sizeof n,"w%010llu",(unsigned long long)cursor);out+=json_string(n);}else out+="null";out+="}";
        return tool(id,out,false);
      }
      std::string wid=jstr(jraw(args,"id"));if(!valid_id(wid))return tool(id,"invalid id (format w0000000000)",true);
      if(engine.tombstoned(wid))return tool(id,"workout_in_trash",true);
      std::string path=engine.path(wid,"json");struct stat st{};
      if(stat(path.c_str(),&st)!=0)return tool(id,"summary_not_found",true);
      if(st.st_size<1||st.st_size>8192)return tool(id,"summary_size_invalid",true);
      FILE *f=fopen(path.c_str(),"rb");if(!f)return tool(id,"summary_open_failed",true);
      std::string out;out.resize(st.st_size);bool good=fread(out.data(),1,out.size(),f)==out.size();if(fclose(f)!=0)good=false;
      return good?tool(id,out,false):tool(id,"summary_read_failed",true);
    }
    if(name=="list_programs"){
      if(busy())return tool(id,"workout_active_or_finalizing: retry after the treadmill is stopped",true);
      FsLease lease;if(!lease.owned)return tool(id,"archive_busy: retry in a moment",true);
      if(!initialized||!treadmill_storage::mounted)return tool(id,"storage_not_ready",true);
      std::string out="{\"items\":[";DIR *d=opendir(plan_dir().c_str());size_t count=0,scanned=0;
      if(d){while(auto *en=readdir(d)){if(++scanned>160)break;std::string n=en->d_name;if(n.size()!=22||n.substr(17)!=".json"||!plan_id(n.substr(0,17)))continue;if(count>=64)break;if(count++)out+=',';out+=json_string(n.substr(0,17).c_str());}closedir(d);}
      return tool(id,out+"]}",false);
    }
    if(name=="get_program"||name=="delete_program"){
      const std::string pid=jstr(jraw(args,"id"));if(!plan_id(pid))return tool(id,"invalid id (format p + 16 hex)",true);
      if(busy())return tool(id,"workout_active_or_finalizing: retry after the treadmill is stopped",true);
      FsLease lease;if(!lease.owned)return tool(id,"archive_busy: retry in a moment",true);
      if(!initialized||!treadmill_storage::mounted)return tool(id,"storage_not_ready",true);
      if(name=="delete_program"){const char *e=plan_delete(pid);return e?tool(id,e,true):tool(id,"{\"deleted\":true}",false);}
      std::string path=plan_dir()+"/"+pid+".json";struct stat st{};
      if(stat(path.c_str(),&st)!=0)return tool(id,"program_not_found",true);
      if(st.st_size<1||st.st_size>8192)return tool(id,"program_size_invalid",true);
      FILE *f=fopen(path.c_str(),"rb");if(!f)return tool(id,"program_open_failed",true);
      std::string o;o.resize(st.st_size);bool good=fread(o.data(),1,o.size(),f)==o.size();if(fclose(f)!=0)good=false;
      return good?tool(id,o,false):tool(id,"program_read_failed",true);
    }
    if(name=="save_program"){
      // Stores a validated program file only. Never starts a workout and never touches the motor.
      const std::string prog=jraw(args,"program");std::string pid;
      if(prog.empty()||!plan_validate(prog,pid))return tool(id,"invalid_program_or_traits: check the schema in the tool description, speed/incline must be on the board grid",true);
      if(busy())return tool(id,"workout_active_or_finalizing: retry after the treadmill is stopped",true);
      FsLease lease;if(!lease.owned)return tool(id,"archive_busy: retry in a moment",true);
      if(!initialized||!treadmill_storage::mounted||!queue)return tool(id,"storage_not_ready",true);
      if(!plan_stopped())return tool(id,"fresh_stop_required: the treadmill must be stopped, retry in a few seconds",true);
      plan_payload=prog;plan_target=pid;Event e{};e.kind=6;mutation_pending=true;job_id=pid;job_operation="program_save";job_error.clear();job_state=1;
      if(xQueueSend(queue,&e,0)!=pdTRUE){mutation_pending=false;job_state=0;plan_payload.clear();return tool(id,"queue_unavailable",true);}
      ++job_number;if(!job_number)++job_number;
      return tool(id,"{\"saved\":\"queued\",\"program_id\":"+json_string(pid.c_str())+",\"note\":\"written in the background; the program appears in the Programs list on the board\"}",false);
    }
    return err(id,-32602,"unknown tool");
  }
  void handleRequest(AsyncWebServerRequest *r) override {
    if(r->method()==HTTP_GET){auto *res=r->beginResponse(405,"application/json","{\"error\":\"POST only\"}");res->addHeader("Allow","POST");r->send(res);return;}
    httpd_req_t *raw=*r;
    if(!body_valid_||body_request_!=raw||body_.size()!=r->contentLength()){body_.clear();body_valid_=false;reply(r,400,err("null",-32700,"bad or too large body"));return;}
    std::string b=std::move(body_);body_.clear();body_valid_=false;
    const std::string id=jraw(b,"id"),method=jstr(jraw(b,"method"));
    if(method.empty()){reply(r,400,err(id.empty()?"null":id,-32600,"invalid request"));return;}
    if(id.empty()){reply(r,202,"");return;} // notification
    const std::string params=jraw(b,"params");
    if(method=="initialize"){
      std::string v=jstr(jraw(params,"protocolVersion"));if(v!="2024-11-05"&&v!="2025-03-26"&&v!="2025-06-18")v="2025-06-18";
      reply(r,200,ok(id,"{\"protocolVersion\":\""+v+"\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"esphome-treadmill\",\"version\":\"0.1\"}}"));return;
    }
    if(method=="ping"){reply(r,200,ok(id,"{}"));return;}
    if(method=="tools/list"){
      reply(r,200,ok(id,"{\"tools\":["
        "{\"name\":\"get_status\",\"description\":\"Recorder state (idle/recording/finalizing) and storage usage.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_live\",\"description\":\"Latest sample of the current or last workout: heart rate, speed, incline level, distance, calories, zone.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"list_workouts\",\"description\":\"Page of saved workout ids, ascending. Use next as after. Unavailable while a workout runs.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"after\":{\"type\":\"string\"},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":20}}}},"
        "{\"name\":\"get_workout_summary\",\"description\":\"Summary JSON of one workout (zones, load, blocks). Unavailable while a workout runs.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}},"
        "{\"name\":\"list_programs\",\"description\":\"Ids of programs stored on the board. Unavailable while a workout runs.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_program\",\"description\":\"Full JSON of one stored program by id.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}},"
        "{\"name\":\"delete_program\",\"description\":\"Delete one stored program by id. Only while the treadmill is stopped. Cannot be undone.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}},"
        "{\"name\":\"save_program\",\"description\":\"Save a custom program on the board; it never starts or moves the motor, the user starts it. Only while stopped. Argument program = {version:1,id:'p'+16 lowercase hex,name:<=80 chars,schema_units:{duration:'s',speed:'km/h',incline:'level'},blocks:[{repeat:1..20,steps:[{duration:1..14400,speed:km/h,incline:level}]}]}. Optional organizing fields: goal (<=40 chars, e.g. endurance), plan (<=40 chars, plan name e.g. Week 43), day ('YYYY-MM-DD', the planned date); use them so the user sees a weekly plan. To be runnable from the page keep the program to about 25 steps after expanding repeats and merging equal neighbours (board plan limit 255 chars); longer programs still save but cannot be started. A step may use zone:1..5 (heart-rate zone) INSTEAD of speed: {duration,zone,incline}; the board's pulse regulator then adapts the speed (slowly, ~0.1-0.5 km/h per 20 s, so a zone step shorter than ~3 min will not settle; without a pulse signal the belt drops to 1 km/h as in Pulse Zone programs). Zones as % of max HR: Z1 50-60 warm-up/recovery, Z2 60-70 aerobic base and fat burn (long steady 20-60 min), Z3 70-80 tempo (10-30 min), Z4 80-90 threshold (3-10 min blocks with recovery), Z5 90-100 short efforts only (30 s-3 min, <10 min total). Use zone steps for long steady blocks and warm-up/cool-down (Z1); use speed steps for short hard intervals. About 80% of weekly time should be Z1-Z2; no hard sessions on consecutive days; check list_workouts/get_workout_summary (trimp, zone_sec) before planning. Max 64 blocks, 64 steps per block, total 14400 s, 8192 bytes; speed and incline must lie on the board's min/max/step grid; same id overwrites.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"program\":{\"type\":\"object\"}},\"required\":[\"program\"]}}"
        "]}"));return;
    }
    if(method=="tools/call"){reply(r,200,call(id,jstr(jraw(params,"name")),jraw(params,"arguments")));return;}
    reply(r,200,err(id,-32601,"method not found"));
  }
};
inline McpHandler handler;
inline void register_handler(){if(esphome::web_server_base::global_web_server_base)esphome::web_server_base::global_web_server_base->add_handler(&handler);}
} // namespace treadmill_mcp
