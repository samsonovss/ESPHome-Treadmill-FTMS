#pragma once
// Portable bounded recorder engine. All methods run on the dedicated I/O worker.
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cerrno>
#include <string>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace treadmill_recorder {
struct Snapshot {
  uint32_t ms=0; int64_t epoch=0; float hr=NAN,speed=0,level=0,max_level=15,max_grade=15,calories=NAN;
  // Energy (cumulative/averages from firmware) and the user profile at workout time.
  float zero_grade=0,fat_kcal=NAN,met_avg=NAN,vo2_avg=NAN,weight=NAN,age=NAN,hrmax=NAN; int male=-1;
  int zone=0,target_zone=0; char program[64]{}; char route[192]{}; bool paused=false; float zgrade=NAN; char phase=0; // zgrade: raw road grade % sent by Zwift (NAN = no Zwift); phase: W warm-up, R main, H work, L rest, C cool-down, 0 none; route: map name at start ("" = none)
};
inline bool valid_id(const std::string &s) { if(s.size()!=11 || s[0]!='w') return false; for(size_t i=1;i<s.size();i++) if(s[i]<'0'||s[i]>'9') return false; return true; }
inline std::string number(double v) { if(!std::isfinite(v)) return "null"; char b[48]; snprintf(b,sizeof b,"%.6f",v); return b; }
inline std::string json_string(const char *s) { std::string r="\""; for(;*s;s++) { unsigned char c=*s; if(c=='"'||c=='\\') r+='\\'; if(c>=32) r+=char(c); } return r+'"'; }
inline std::string csv_string(const char *s) { std::string r="\""; for(;*s;s++) { if(*s=='"') r+='"'; if(*s!='\r'&&*s!='\n') r+=*s; } return r+'"'; }
inline std::string epoch_text(int64_t epoch) {
  if(epoch<=0)return "";
  time_t raw=static_cast<time_t>(epoch);tm utc{};char out[32]{};
  if(!gmtime_r(&raw,&utc)||!strftime(out,sizeof out,"%Y-%m-%dT%H:%M:%SZ",&utc))return "";
  return out;
}
inline int64_t parse_epoch(const char *text) {
  if(strlen(text)!=20||text[19]!='Z')return 0;
  int y,m,d,h,minute,sec;char tail=0;
  if(sscanf(text,"%4d-%2d-%2dT%2d:%2d:%2dZ%c",&y,&m,&d,&h,&minute,&sec,&tail)!=6)return 0;
  if(y<2000||y>2200||m<1||m>12||d<1||d>31||h<0||h>23||minute<0||minute>59||sec<0||sec>59)return 0;
  y-=m<=2;const int era=y/400;const unsigned yo=unsigned(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  const unsigned doe=yo*365+yo/4-yo/100+doy;
  const int64_t result=(int64_t(era)*146097+doe-719468)*86400+h*3600+minute*60+sec;
  return epoch_text(result)==text?result:0;
}
inline constexpr const char *COLUMNS="timestamp,heart_rate,speed_kmh,incline_pct,distance_km,time_s,calories,status,program,zone,incline_level,target_zone,elapsed_s\n";
class Engine {
 public:
  std::string base,error,id; uint64_t sequence=0; bool ready=false,active=false,wall_valid=true;
  // 17.10 compact CSV: same columns, but a cell left empty = «same as before» (timestamp: last one + elapsed; time_s: = elapsed).
  int64_t w_epoch=0; double w_el=0; int w_zone=-1,w_tz=-1,w_status=-1; float w_lvl=NAN,w_g=NAN; bool w_first=true;
  FILE *file=nullptr; Snapshot previous{}; double timer=0,elapsed=0,distance=0,hrsum=0,speedsum=0,gradesum=0,maxhr=0; unsigned samples=0,hrcount=0,running_samples=0,unflushed=0;
  int64_t start_epoch=0,end_epoch=0; float cal=NAN,max_level=15,max_grade=15; char program[64]{}; char route[192]{};
  float zero_grade=0,fat_kcal=NAN,met_avg=NAN,vo2_avg=NAN,p_weight=NAN,p_age=NAN,p_hrmax=NAN; int p_male=-1;
  // Insights for History: seconds per firmware HR zone (0 = below Z1), target zone, max speed, climb.
  double zone_sec[6]{},max_speed=0,ascent=0; int target_zone=0;
  std::vector<std::pair<float,char>> phases; char last_phase=0;
  std::vector<std::pair<float,float>> zprof; double zend=-1; // Zwift road profile [km, grade %] (on change ≥0.2 % or every 0.25 km) // program phase transitions (timer s), for History bands
  // 5-minute blocks of running time [end timer s, avg speed, avg grade %, avg HR (0 = no HR)] for Progress (steady-state HR trend).
  std::vector<std::array<float,4>> blocks; double b_t=0,b_sp=0,b_gr=0,b_hr=0,b_hrt=0;
  void flush_block(){if(b_t>=60&&blocks.size()<60)blocks.push_back({float(timer),float(b_sp/b_t),float(b_gr/b_t),b_hrt>=b_t*0.5?float(b_hr/b_hrt):0.0f});b_t=b_sp=b_gr=b_hr=b_hrt=0;}
  void reset_insights(){for(auto &z:zone_sec)z=0;max_speed=ascent=0;target_zone=0;phases.clear();last_phase=0;zprof.clear();zend=-1;blocks.clear();b_t=b_sp=b_gr=b_hr=b_hrt=0;}
  explicit Engine(std::string path):base(std::move(path)) {}
  ~Engine(){ if(file) fclose(file); }
  std::string path(const std::string &i,const char *ext) const { return base+"/"+i+"."+ext; }
  bool fail(const char *reason) { error=reason; active=false; if(file) { fclose(file);file=nullptr; } return false; }
  std::string trash_dir(const std::string &i) const { return base+"/.trash/"+i; }
  // A single-byte manifest is synced before moving any original file. Retained
  // manifests also serve as persistent sequence tombstones (never reused IDs).
  char trash_state(const std::string &i) const {
    FILE *f=fopen((trash_dir(i)+"/manifest").c_str(),"rb");
    if(!f)return errno==ENOENT?'0':'!';
    int c=fgetc(f);bool ok=!ferror(f);if(fclose(f)!=0)ok=false;
    return ok&&(c=='T'||c=='R'||c=='D'||c=='P'||c=='G')?char(c):'!';
  }
  bool tombstoned(const std::string &i) const { char c=trash_state(i);return c!='0'&&c!='D'; }
  bool manifest(const std::string &i,char state) {
    int fd=open((trash_dir(i)+"/manifest").c_str(),O_WRONLY|O_CREAT,0600);
    if(fd<0){error="manifest_open_failed";return false;}
    bool ok=write(fd,&state,1)==1;if(fsync(fd)!=0)ok=false;if(close(fd)!=0)ok=false;
    if(!ok)error="manifest_sync_failed";
    return ok;
  }
  bool move_selected(const std::string &i,bool restore) {
    if(!valid_id(i)){error="invalid_id";return false;}
    if(active||file){error="workout_active_or_finalizing";return false;}
    if(mkdir((base+"/.trash").c_str(),0700)!=0&&errno!=EEXIST){error="trash_directory_failed";return false;}
    if(mkdir(trash_dir(i).c_str(),0700)!=0&&errno!=EEXIST){error="trash_directory_failed";return false;}
    char state=trash_state(i);if(state=='!'){error="manifest_read_failed";return false;}
    if(restore&&state=='0'){error="not_in_trash";return false;}
    if(restore&&state=='D')return true;
    const char *exts[]={"csv","json","meta","csv.tmp","json.tmp","meta.tmp","csvz","csvz.tmp"};
    bool any=false;
    // Preflight every destination: rename must never replace existing bytes.
    for(auto ext:exts){struct stat a{},b{};auto root=path(i,ext),bin=trash_dir(i)+"/"+i+"."+ext;
      bool x=stat(root.c_str(),&a)==0;if(!x&&errno!=ENOENT){error="archive_stat_failed";return false;}
      bool y=stat(bin.c_str(),&b)==0;if(!y&&errno!=ENOENT){error="trash_stat_failed";return false;}
      if((x&&!S_ISREG(a.st_mode))||(y&&!S_ISREG(b.st_mode))|| (x&&y)){error="destination_conflict";return false;}any=any||x||y;
    }
    if(!any){if(!restore&&state=='T')return true;error="workout_not_found";return false;}
    if(!manifest(i,restore?'R':'T'))return false;
    for(auto ext:exts){auto root=path(i,ext),bin=trash_dir(i)+"/"+i+"."+ext;auto from=restore?bin:root,to=restore?root:bin;
      struct stat st{};if(stat(from.c_str(),&st)!=0){if(errno==ENOENT)continue;error="move_stat_failed";return false;}
      if(rename(from.c_str(),to.c_str())!=0){error="archive_move_failed";return false;}
    }
    if(restore&&!manifest(i,'D'))return false;
    return true;
  }
  // Permanent delete (12.10): only from the trash. 'P' is synced first (the manifest stays as the ID tombstone),
  // then the trashed files are unlinked; an interrupted purge is finished by scan_trash at boot.
  bool purge_files(const std::string &i) {
    const char *exts[]={"csv","json","meta","csv.tmp","json.tmp","meta.tmp","csvz","csvz.tmp"};bool ok=true;
    for(auto ext:exts){auto bin=trash_dir(i)+"/"+i+"."+ext,root=path(i,ext);
      if(unlink(bin.c_str())!=0&&errno!=ENOENT)ok=false;if(unlink(root.c_str())!=0&&errno!=ENOENT)ok=false;}
    if(!ok)error="purge_unlink_failed";return ok;
  }
  bool purge(const std::string &i) {
    if(!valid_id(i)){error="invalid_id";return false;}
    if(active||file){error="workout_active_or_finalizing";return false;}
    // one step from the archive too (12.10): no trash stop on the way
    if(mkdir((base+"/.trash").c_str(),0700)!=0&&errno!=EEXIST){error="trash_directory_failed";return false;}
    if(mkdir(trash_dir(i).c_str(),0700)!=0&&errno!=EEXIST){error="trash_directory_failed";return false;}
    char state=trash_state(i);if(state=='!'){error="manifest_read_failed";return false;}if(state=='G')return true;if(state=='P')return purge_files(i)&&manifest(i,'G');
    if(!manifest(i,'P'))return false;if(!purge_files(i))return false;if(!manifest(i,'G'))return false;prune_tombstones();return true;
  }
  // 17.10: every purged workout left a .trash/<id>/manifest tombstone (~8–12 KB of LittleFS metadata each, ~1.3 MB for 150).
  // IDs are never reused because the sequence comes from the highest ID seen — so only the highest finished tombstone is needed.
  // Drop the rest: 'G' (purged) except the highest one, and 'D' (restored — the files are back in the archive). Best effort.
  void prune_tombstones() {
    auto directory=base+"/.trash";DIR *d=opendir(directory.c_str());if(!d)return;
    std::vector<std::string> drop;std::string keep;
    for(;;){auto *e=readdir(d);if(!e)break;std::string i=e->d_name;if(!valid_id(i))continue;char st=trash_state(i);
      if(st=='D')drop.push_back(i);else if(st=='G'){if(keep.empty()||i>keep){if(!keep.empty())drop.push_back(keep);keep=i;}else drop.push_back(i);}
      vTaskDelay(1);}
    closedir(d);
    for(auto &i:drop){if(unlink((trash_dir(i)+"/manifest").c_str())==0)rmdir(trash_dir(i).c_str());vTaskDelay(pdMS_TO_TICKS(2));}
  }
  bool scan_trash() {
    auto directory=base+"/.trash";DIR *d=opendir(directory.c_str());
    if(!d){if(errno==ENOENT)return true;error="trash_scan_failed";return false;}
    bool ok=true;
    for(;;){errno=0;auto *e=readdir(d);if(!e){if(errno)ok=false;break;}std::string i=e->d_name;if(!valid_id(i))continue;
      sequence=std::max(sequence,uint64_t(strtoull(i.substr(1).c_str(),nullptr,10)));
      char state=trash_state(i);if(state=='T'||state=='R'){if(!move_selected(i,state=='R'))ok=false;}else if(state=='P'){if(!purge_files(i)||!manifest(i,'G'))ok=false;}else if(state=='!')ok=false;
      vTaskDelay(1);
    }
    if(closedir(d)!=0)ok=false;
    if(!ok&&error.empty())error="trash_recovery_failed";
    return ok;
  }
  bool initialize() {
    DIR *d=opendir(base.c_str()); if(!d) return fail("storage_unavailable");
    errno=0; while(auto *e=readdir(d)) { std::string n=e->d_name; if(n.size()>=15&&valid_id(n.substr(0,11))&&n[11]=='.') sequence=std::max(sequence,uint64_t(strtoull(n.substr(1,10).c_str(),nullptr,10))); }
    const int scan_error=errno; if(closedir(d)!=0||scan_error) return fail("scan_failed");
    if(!scan_trash()) return fail("trash_recovery_failed");
    prune_tombstones();
    // Recovery is streaming, bounded memory, only own CSV files; never truncate/delete.
    d=opendir(base.c_str()); if(!d) return fail("scan_failed");
    while(true) { errno=0;auto *e=readdir(d);if(!e){if(errno)error="recovery_scan_failed";break;}std::string n=e->d_name; if(n.size()!=15||n.substr(11)!=".csv"||!valid_id(n.substr(0,11))) continue; struct stat st{}; auto i=n.substr(0,11); if(tombstoned(i)) continue; if(stat(path(i,"json").c_str(),&st)==0) continue; if(errno!=ENOENT) { error="recovery_stat_failed";continue; } recover(i); }
    if(closedir(d)!=0) return fail("scan_close_failed");
    ready=true; return true;
  }
  bool checkpoint() { if(!file) return false; if(fflush(file)!=0||fsync(fileno(file))!=0) return fail(errno==ENOSPC?"storage_full":"checkpoint_failed"); unflushed=0;return true; }
  bool start(const Snapshot &s) {
    if(active) return true;
    if(!ready) return fail("storage_not_ready");
    if(sequence>=UINT32_MAX) return fail("id_exhausted");
    char name[24]; snprintf(name,sizeof name,"w%010llu",(unsigned long long)++sequence);id=name;
    int fd=open(path(id,"csv").c_str(),O_WRONLY|O_CREAT|O_EXCL,0600); if(fd<0) return fail(errno==ENOSPC?"storage_full":"create_failed");
    file=fdopen(fd,"wb"); if(!file) {close(fd);return fail("fdopen_failed");} setvbuf(file,nullptr,_IOFBF,4096);
    timer=elapsed=distance=hrsum=speedsum=gradesum=maxhr=0; samples=hrcount=running_samples=unflushed=0;reset_insights();
    start_epoch=s.epoch;end_epoch=s.epoch;wall_valid=s.epoch>0;previous=s;cal=s.calories;max_level=s.max_level;max_grade=s.max_grade; memcpy(program,s.program,sizeof program);memcpy(route,s.route,sizeof route);route[sizeof route-1]=0;error.clear();active=true;
    zero_grade=std::isfinite(s.zero_grade)?s.zero_grade:0;fat_kcal=met_avg=vo2_avg=NAN;p_weight=s.weight;p_age=s.age;p_hrmax=s.hrmax;p_male=s.male;
    if(fputs(COLUMNS,file)==EOF) return fail("header_write_failed");
    w_epoch=0;w_el=0;w_zone=w_tz=w_status=-1;w_lvl=w_g=NAN;w_first=true;
    int meta=open(path(id,"meta").c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(meta<0)return fail(errno==ENOSPC?"storage_full":"metadata_open_failed");
    char m[128];int len=snprintf(m,sizeof m,"%.6f %.6f %.6f\n",max_level,max_grade,zero_grade);bool ok=write(meta,m,len)==len;
    if(fsync(meta)!=0)ok=false;
    if(close(meta)!=0)ok=false;
    if(!ok)return fail(errno==ENOSPC?"storage_full":"metadata_write_failed");
    return sample(s);
  }
  bool sample(const Snapshot &s) {
    if(!active) return false;
    double dt=double(uint32_t(s.ms-previous.ms))/1000.0; elapsed+=dt;
    if(!previous.paused) { double dd=std::max(0.0f,previous.speed)*dt/3600.0; timer+=dt;distance+=dd;
      if(std::isfinite(previous.hr)&&previous.hr>0&&previous.zone>=0&&previous.zone<=5) zone_sec[previous.zone]+=dt;
      double bg=0;
      if(max_level>0&&std::isfinite(max_grade)){float pg=zero_grade+std::max(0.0f,std::min(previous.level,max_level))/max_level*(max_grade-zero_grade);bg=pg;if(pg>0)ascent+=dd*1000.0*pg/100.0;}
      b_t+=dt;b_sp+=std::max(0.0f,previous.speed)*dt;b_gr+=bg*dt;if(std::isfinite(previous.hr)&&previous.hr>0){b_hr+=previous.hr*dt;b_hrt+=dt;}
      if(b_t>=300)flush_block(); }
    if(s.target_zone>=1&&s.target_zone<=5) target_zone=s.target_zone;
    if(std::isfinite(s.zgrade)&&!s.paused){if((zprof.empty()||std::fabs(s.zgrade-zprof.back().second)>=0.2f||distance-zprof.back().first>=0.25)&&zprof.size()<1500)zprof.emplace_back(float(distance),s.zgrade);zend=distance;}
    if(s.phase!=last_phase&&phases.size()<400){phases.emplace_back(float(timer),s.phase);last_phase=s.phase;}
    wall_valid=wall_valid&&s.epoch>0&&(previous.epoch<=0||s.epoch>=previous.epoch);end_epoch=s.epoch;cal=s.calories;
    if(std::isfinite(s.fat_kcal)){fat_kcal=s.fat_kcal;}
    if(std::isfinite(s.met_avg)){met_avg=s.met_avg;}
    if(std::isfinite(s.vo2_avg)){vo2_avg=s.vo2_avg;}
    float grade=(max_level>0&&std::isfinite(max_grade))?zero_grade+std::max(0.0f,std::min(s.level,max_level))/max_level*(max_grade-zero_grade):NAN;
    char t[32]="",h[32]="",c[32]="",g[32]="";
    if(s.epoch>0) snprintf(t,sizeof t,"%s",epoch_text(s.epoch).c_str());
    if(std::isfinite(s.hr)&&s.hr>0) {snprintf(h,sizeof h,"%.1f",s.hr);hrsum+=s.hr;hrcount++;maxhr=std::max(maxhr,double(s.hr));}
    if(std::isfinite(cal)) snprintf(c,sizeof c,"%.1f",cal);
    if(std::isfinite(grade)) snprintf(g,sizeof g,"%.3f",grade);
    if(!s.paused) { max_speed=std::max(max_speed,double(s.speed));speedsum+=s.speed; if(std::isfinite(grade)) gradesum+=grade;running_samples++; }
    auto p=csv_string(s.program);
    {// compact row: repeats left empty (see w_* above); numbers only as precise as the sensors are
     char T[32]="",H[8]="",G[16]="",TM[16]="",ST[8]="",Z[6]="",LV[10]="",TZ[6]="";
     if(s.epoch>0&&(w_first||w_epoch<=0||std::llabs((long long)(s.epoch-w_epoch)-llround(elapsed-w_el))>1||elapsed-w_el>=300)){snprintf(T,sizeof T,"%s",t);w_epoch=s.epoch;w_el=elapsed;}
     if(h[0])snprintf(H,sizeof H,"%.0f",s.hr);
     if(g[0]&&(w_first||!std::isfinite(w_g)||std::fabs(grade-w_g)>=0.05f)){snprintf(G,sizeof G,"%.1f",grade);w_g=grade;}
     if(std::fabs(timer-elapsed)>=0.05)snprintf(TM,sizeof TM,"%.1f",timer);
     const int stv=s.paused?1:0;if(w_first||stv!=w_status){snprintf(ST,sizeof ST,"%s",stv?"paused":"running");w_status=stv;}
     if(w_first||s.zone!=w_zone){snprintf(Z,sizeof Z,"%d",s.zone);w_zone=s.zone;}
     if(w_first||!std::isfinite(w_lvl)||std::fabs(s.level-w_lvl)>=0.05f){snprintf(LV,sizeof LV,"%.1f",s.level);w_lvl=s.level;}
     if(w_first||s.target_zone!=w_tz){snprintf(TZ,sizeof TZ,"%d",s.target_zone);w_tz=s.target_zone;}
     if(fprintf(file,"%s,%s,%.1f,%s,%.4f,%s,%s,%s,%s,%s,%s,%s,%.1f\n",T,H,s.speed,G,distance,TM,c,ST,w_first?p.c_str():"",Z,LV,TZ,elapsed)<0) return fail(errno==ENOSPC?"storage_full":"sample_write_failed");
     w_first=false;}
    previous=s;samples++;unflushed++; if(unflushed>=5) return checkpoint();return true;
  }
  std::string phases_json() const { std::string o; for(size_t i=0;i<phases.size();i++){char b[32];snprintf(b,sizeof b,"%s[%.0f,\"%c\"]",i?",":"",phases[i].first,phases[i].second?phases[i].second:'0');o+=b;} return o; }
  std::string zwift_json() const { if(zprof.size()<2||zend<=zprof.front().first) return "null"; std::string o="["; for(size_t i=0;i<zprof.size();i++){char b[40];snprintf(b,sizeof b,"%s[%.3f,%.2f]",i?",":"",zprof[i].first,zprof[i].second);o+=b;} char b[48];snprintf(b,sizeof b,",[%.3f,%.2f]]",zend,zprof.back().second);return o+b; }
  std::string zones_json() const { std::string o; for(int i=0;i<6;i++){char b[24];snprintf(b,sizeof b,"%s%.0f",i?",":"",zone_sec[i]);o+=b;} return o; }
  std::string blocks_json() const { std::string o; for(size_t i=0;i<blocks.size();i++){char b[80];snprintf(b,sizeof b,"%s[%.0f,%.1f,%.1f,%.0f]",i?",":"",blocks[i][0],blocks[i][1],blocks[i][2],blocks[i][3]);o+=b;} return o; }
  double trimp() const { double t=0; for(int i=1;i<6;i++)t+=zone_sec[i]*i; return t/60.0; }
  std::string summary(bool interrupted) const {
    return "{\"schema_version\":1,\"timestamp_format\":\"iso8601_utc\",\"id\":"+json_string(id.c_str())+",\"start\":"+(start_epoch>0?json_string(epoch_text(start_epoch).c_str()):"null")+",\"end\":"+(end_epoch>0?json_string(epoch_text(end_epoch).c_str()):"null")+",\"duration_sec\":"+number(timer)+",\"elapsed_sec\":"+number(elapsed)+",\"samples\":"+std::to_string(samples)+",\"distance_km\":"+number(distance)+",\"calories\":"+number(cal)+",\"avg_speed_kmh\":"+number(running_samples?speedsum/running_samples:NAN)+",\"avg_incline_pct\":"+number(running_samples?gradesum/running_samples:NAN)+",\"avg_heart_rate\":"+number(hrcount?hrsum/hrcount:NAN)+",\"max_heart_rate\":"+number(hrcount?maxhr:NAN)+",\"wall_time_valid\":"+(wall_valid?"true":"false")+",\"interrupted\":"+(interrupted?"true":"false")+",\"grade_estimated\":true,\"distance_estimated\":true,\"calories_estimated\":true,\"calories_source\":\"firmware_total_calories\",\"speed_source\":\"live_target\",\"calibration_max_level\":"+number(max_level)+",\"calibration_real_max_grade\":"+number(max_grade)+",\"calibration_zero_grade\":"+number(zero_grade)+",\"fat_g\":"+number(fat_kcal/9.0)+",\"carbs_g\":"+number(std::isfinite(cal)&&std::isfinite(fat_kcal)?std::max(0.0,double(cal)-double(fat_kcal))/4.0:NAN)+",\"avg_met\":"+number(met_avg)+",\"avg_vo2\":"+number(vo2_avg)+",\"energy_source\":\"firmware_per_second_acsm\",\"profile\":{\"weight_kg\":"+number(p_weight)+",\"age\":"+number(p_age)+",\"sex\":"+(p_male==1?"\"male\"":p_male==0?"\"female\"":"null")+",\"hr_max\":"+number(p_hrmax)+"},\"zone_sec\":["+zones_json()+"],\"target_zone\":"+(target_zone?std::to_string(target_zone):std::string("null"))+",\"max_speed_kmh\":"+number(running_samples?max_speed:NAN)+",\"ascent_m\":"+number(ascent)+",\"phases\":["+phases_json()+"],\"trimp\":"+number(trimp())+",\"blocks_sec\":300,\"blocks\":["+blocks_json()+"],\"zwift_profile\":"+zwift_json()+",\"route\":"+(route[0]?json_string(route):std::string("null"))+",\"program\":"+json_string(program)+"}\n";
  }
  bool write_summary(bool interrupted) {
    // LittleFS VFS exposes rename: write+sync+close first, rename within same mount.
    auto tmp=path(id,"json.tmp");int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600); if(fd<0) return fail(errno==ENOSPC?"storage_full":"summary_open_failed");auto text=summary(interrupted);
    size_t off=0;bool ok=true; while(off<text.size()) {ssize_t n=write(fd,text.data()+off,text.size()-off);if(n<=0){ok=false;break;}off+=size_t(n);} if(fsync(fd)!=0) ok=false; if(close(fd)!=0) ok=false;
    if(!ok) return fail(errno==ENOSPC?"storage_full":"summary_write_failed");
    if(rename(tmp.c_str(),path(id,"json").c_str())!=0) return fail("summary_rename_failed");
    return true;
  }
  bool finish(const Snapshot &s,bool interrupted=false) { if(!active)return true; if(!sample(s)||!checkpoint())return false;flush_block();FILE *f=file;file=nullptr;active=false;if(fclose(f)!=0)return fail(errno==ENOSPC?"storage_full":"csv_close_failed");return write_summary(interrupted); }
  void recover(const std::string &i) {
    FILE *f=fopen(path(i,"csv").c_str(),"rb");if(!f){error="recovery_open_failed";return;}
    id=i;timer=elapsed=distance=hrsum=speedsum=gradesum=maxhr=0;samples=hrcount=running_samples=0;start_epoch=end_epoch=0;wall_valid=true;cal=NAN;max_level=max_grade=NAN;program[0]=0;route[0]=0;
    zero_grade=0;fat_kcal=met_avg=vo2_avg=p_weight=p_age=p_hrmax=NAN;p_male=-1;reset_insights();double pt=NAN,pd=NAN,pg=NAN,ph=NAN;int pz=-1;
    FILE *meta=fopen(path(i,"meta").c_str(),"rb");if(meta){{int k=fscanf(meta,"%f %f %f",&max_level,&max_grade,&zero_grade);if(k<2){max_level=max_grade=NAN;}if(k<3)zero_grade=0;}if(ferror(meta))error="recovery_metadata_read_failed";if(fclose(meta)!=0)error="recovery_metadata_close_failed";}else if(errno!=ENOENT)error="recovery_metadata_open_failed";
    char line[768]; bool header=fgets(line,sizeof line,f)&&strcmp(line,COLUMNS)==0;
    while(header&&fgets(line,sizeof line,f)) {
      if(!strchr(line,'\n')) { // Ignore incomplete trailing row; overlong non-final rows are corruption.
        if(!feof(f)) error="recovery_partial_or_overlong_row";
        break;
      }
      // CSV tokenizer supporting quoted programs, doubled quotes, fixed columns.
      char fields[13][128]{};int col=0,pos=0;bool quote=false,bad=false;
      for(size_t k=0;line[k]&&line[k]!='\n';k++){char x=line[k];if(x=='"'){if(quote&&line[k+1]=='"'){k++;x='"';}else{quote=!quote;continue;}}else if(x==','&&!quote){if(++col>=13){bad=true;break;}pos=0;continue;}if(pos>=127){bad=true;break;}fields[col][pos++]=x;}
      if(bad||quote||col!=12) {error="recovery_invalid_row";break;}
      // compact rows (17.10): empty cell = previous value; empty time_s = elapsed; empty timestamp = last one + elapsed delta
      {static char prev[13][128];static double pep,pel;if(samples==0){memset(prev,0,sizeof prev);pep=NAN;pel=NAN;}
       const double el=fields[12][0]?strtod(fields[12],nullptr):NAN;
       if(!fields[0][0]&&std::isfinite(pep)&&std::isfinite(el)){snprintf(fields[0],128,"%s",epoch_text(int64_t(pep+llround(el-pel))).c_str());}
       else if(fields[0][0]){double e0=double(parse_epoch(fields[0]));if(std::isfinite(e0)){pep=e0;pel=el;}}
       if(!fields[5][0]&&fields[12][0])memcpy(fields[5],fields[12],128);
       for(int k:{3,7,9,10,11}){if(fields[k][0])memcpy(prev[k],fields[k],128);else memcpy(fields[k],prev[k],128);}}
      auto val=[&](int c){return fields[c][0]?strtod(fields[c],nullptr):NAN;};double ep=double(parse_epoch(fields[0]));if(samples==0)start_epoch=std::isfinite(ep)?int64_t(ep):0;end_epoch=std::isfinite(ep)?int64_t(ep):0;wall_valid=wall_valid&&end_epoch>0;
      timer=val(5);elapsed=val(12);distance=val(4);cal=val(6); double h=val(1);if(std::isfinite(h)&&h>0){hrsum+=h;hrcount++;maxhr=std::max(maxhr,h);}if(strcmp(fields[7],"running")==0){speedsum+=val(2);gradesum+=val(3);running_samples++;max_speed=std::max(max_speed,val(2));}
      {double t=val(5),d=val(4);if(std::isfinite(pt)&&std::isfinite(t)&&t>pt){if(std::isfinite(ph)&&ph>0&&pz>=0&&pz<=5)zone_sec[pz]+=t-pt;if(std::isfinite(pg)&&pg>0&&std::isfinite(pd)&&std::isfinite(d)&&d>pd)ascent+=(d-pd)*1000.0*pg/100.0;}
       pt=t;pd=d;pg=val(3);ph=h;pz=fields[9][0]?atoi(fields[9]):-1;int tz=fields[11][0]?atoi(fields[11]):0;if(tz>=1&&tz<=5)target_zone=tz;}if(samples==0)snprintf(program,sizeof program,"%.63s",fields[8]);samples++;
    }
    bool ok=!ferror(f);if(fclose(f)!=0)ok=false;if(!header||!ok){error="recovery_read_failed";return;}write_summary(true);
  }
};
}
