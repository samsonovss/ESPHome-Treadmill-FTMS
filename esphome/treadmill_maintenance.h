#pragma once
#include "esphome/components/web_server_base/web_server_base.h"
#include <nvs.h>
#include <esp_random.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace treadmill_maintenance {
struct Record {
  uint64_t total_mm{}, last_mm{};
  uint32_t last_at{}, km{}, months{}, marked{};
};
inline Record record;
inline std::mutex lock;
inline nvs_handle_t storage;
inline bool ready{}, stopped{};
inline uint64_t saved_mm{}, previous_mm{};
inline uint32_t now_s{}, last_attempt{}, completed{};
inline bool failed{};
inline const uint32_t generation=esp_random();
struct Action { uint32_t id{}, km{}, months{}; bool mark{}; };
inline Action pending;
inline uint32_t next_id{};

inline void initialize() {
  if (nvs_open("tm_maintenance", NVS_READWRITE, &storage) != ESP_OK) return;
  Record loaded{};
  size_t size=sizeof loaded;
  auto err=nvs_get_blob(storage,"state_v1",&loaded,&size);
  if (err!=ESP_ERR_NVS_NOT_FOUND && (err!=ESP_OK || size!=sizeof loaded ||
      loaded.total_mm>100000000000000ULL || loaded.last_mm>loaded.total_mm ||
      loaded.marked>1 || loaded.km>100000 || loaded.months>120)) return;
  std::lock_guard<std::mutex> g(lock);
  record=loaded; saved_mm=record.total_mm; ready=true;
}
// Observe the existing distance sample, independent of recorder and profiles.
inline void observe(float km) {
  if (!std::isfinite(km) || km<0 || km>100000) return;
  const auto mm=static_cast<uint64_t>(std::llround(double(km)*1000000));
  std::lock_guard<std::mutex> g(lock);
  if (ready && mm>previous_mm) record.total_mm+=mm-previous_mm;
  if (mm==0 || mm>previous_mm) previous_mm=mm;
}
inline bool save(const Record &r) {
  return nvs_set_blob(storage,"state_v1",&r,sizeof r)==ESP_OK && nvs_commit(storage)==ESP_OK;
}
// Main loop only. No NVS writes while the treadmill is moving or paused.
inline void tick(bool idle, uint32_t now, uint32_t ms) {
  Action job;
  Record target;
  {
    std::lock_guard<std::mutex> g(lock);
    stopped=idle; now_s=now;
    if (!ready) return;
    if (!idle) {
      if (pending.id) { completed=pending.id; failed=true; pending={}; }
      return;
    }
    job=pending;
    if (!job.id && (record.total_mm==saved_mm || uint32_t(ms-last_attempt)<5000)) return;
    target=record;
  }
  last_attempt=ms;
  if (job.mark) { target.marked=1; target.last_mm=target.total_mm; target.last_at=now; }
  else if (job.id && !job.mark) { target.km=job.km; target.months=job.months; }
  const bool ok=save(target);
  std::lock_guard<std::mutex> g(lock);
  if (ok) { record=target; saved_mm=target.total_mm; }
  if (job.id) { completed=job.id; failed=!ok; pending={}; }
}
inline std::string state() {
  const auto r=record;
  auto optional=[](bool valid,uint64_t v){return valid?std::to_string(v):std::string("null");};
  return "{\"total_m\":"+std::to_string(r.total_mm/1000)+",\"last_m\":"+optional(r.marked,r.last_mm/1000)
    +",\"last_at\":"+optional(r.last_at,r.last_at)+",\"now\":"+optional(now_s,now_s)
    +",\"interval_km\":"+std::to_string(r.km)+",\"interval_months\":"+std::to_string(r.months)
    +",\"generation\":"+std::to_string(generation)+",\"completed\":"+std::to_string(completed)
    +",\"failed\":"+(failed?"true":"false")+"}";
}
inline bool number(const char *&q,uint32_t max,uint32_t &v) {
  if (*q<'0'||*q>'9') return false;
  v=0;
  while (*q>='0'&&*q<='9') { v=v*10+(*q++-'0'); if (v>max) return false; }
  return true;
}
class Handler: public AsyncWebHandler {
  void reply(AsyncWebServerRequest *r,int code,const std::string &body) {
    auto *res=r->beginResponse(code,"application/json",body);
    res->addHeader("Cache-Control","no-store"); r->send(res);
  }
 public:
  bool canHandle(AsyncWebServerRequest *r) const override {
    char b[AsyncWebServerRequest::URL_BUF_SIZE];
    return r->url_to(std::span<char,AsyncWebServerRequest::URL_BUF_SIZE>(b,sizeof(b)))=="/api/maintenance" &&
      (r->method()==HTTP_GET || r->method()==HTTP_POST);
  }
  void handleRequest(AsyncWebServerRequest *r) override {
    int code=200;
    std::string body;
    {
      std::lock_guard<std::mutex> g(lock);
      if (!ready) { code=503; body="{\"error\":\"storage_unavailable\"}"; }
      else if (r->method()==HTTP_GET) body=state();
      else {
        const auto header=r->get_header("X-Treadmill-Maintenance");
        const auto site=r->get_header("Sec-Fetch-Site");
        httpd_req_t *raw=*r; const char *q=std::strchr(raw->uri,'?');
        Action action;
        bool valid=false;
        if (q && std::strcmp(q,"?action=mark")==0) valid=action.mark=true;
        if (q && std::strncmp(q,"?action=config&km=",18)==0) {
          q+=18;
          if (number(q,100000,action.km) && std::strncmp(q,"&months=",8)==0) {
            q+=8; valid=number(q,120,action.months) && *q=='\0';
          }
        }
        if (!header.has_value() || header.value()!="1" || (site.has_value() && site.value()!="same-origin")) { code=403; body="{}"; }
        else if (!valid) { code=400; body="{}"; }
        else if (!stopped || pending.id) { code=409; body="{}"; }
        else {
          action.id=++next_id; if (!action.id) action.id=++next_id;
          pending=action; code=202;
          body="{\"generation\":"+std::to_string(generation)+",\"target\":"+std::to_string(action.id)+"}";
        }
      }
    }
    reply(r,code,body);
  }
};
inline Handler handler;
inline void register_handler() {
  if (esphome::web_server_base::global_web_server_base)
    esphome::web_server_base::global_web_server_base->add_handler(&handler);
}
}
