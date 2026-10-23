// Motor-board incline command format (10.10). Default = the original console protocol "[SETINC:%03d]", value in tenths.
// Other boards: change the template (one %d with optional zero-pad width) and the scale in Settings → Incline.
// Test: a raw command (SETINC only) replaces the periodic incline command for 15 s while the belt runs.
#pragma once
#include <string>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#ifndef TREADMILL_MOTOR_TEST
#include "esphome/core/hal.h"
#endif
namespace treadmill_motor {
inline const char *DEFAULT_TPL = "[SETINC:%03d]";
inline std::string tpl = DEFAULT_TPL;
inline int scale = 10;            // command value = round(incline % * scale)
inline uint32_t raw_until = 0;    // millis() deadline of the raw test
inline std::string raw_cmd;
// "[SETINC:" … "]" with exactly one conversion "%d" / "%0Nd" (N 1–6) and no other '%'.
inline bool valid_tpl(const std::string &t) {
  if (t.size() < 11 || t.size() > 24 || t.rfind("[SETINC:", 0) != 0 || t.back() != ']') return false;
  int convs = 0;
  for (size_t i = 0; i < t.size(); i++) {
    if (t[i] != '%') continue;
    size_t j = i + 1;
    if (j < t.size() && t[j] == '0') j++;
    if (j < t.size() && t[j] >= '1' && t[j] <= '6') j++;
    if (j >= t.size() || t[j] != 'd') return false;
    convs++; i = j;
  }
  return convs == 1;
}
// Raw test command: "[SETINC:" + 1–8 chars of digits / '.' / '-' + "]".
inline bool valid_raw(const std::string &s) {
  if (s.size() < 10 || s.size() > 20 || s.rfind("[SETINC:", 0) != 0 || s.back() != ']') return false;
  for (size_t i = 8; i + 1 < s.size(); i++) { char c = s[i]; if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) return false; }
  return true;
}
// Probe (command search): free form "[NAME]" or "[NAME:value]" — NAME 2–10 of A–Z/0–9/_, value up to 10 printable chars
// without brackets; total ≤ 24. Never SETSPD (speed is not probed).
inline bool valid_probe(const std::string &s) {
  if (s.size() < 4 || s.size() > 24 || s[0] != '[' || s.back() != ']') return false;
  size_t c = s.find(':'), end = c == std::string::npos ? s.size() - 1 : c;
  if (end < 3 || end > 11) return false;
  for (size_t i = 1; i < end; i++) { char ch = s[i]; if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')) return false; }
  if (s.compare(1, end - 1, "SETSPD") == 0) return false;
  if (c != std::string::npos) {
    if (s.size() - 1 - (c + 1) > 10) return false;
    for (size_t i = c + 1; i + 1 < s.size(); i++) { char ch = s[i]; if (ch < 33 || ch > 126 || ch == '[' || ch == ']') return false; }
  }
  return true;
}
inline bool is_reset(const std::string &s) { return s.rfind("[RESET", 0) == 0; }
// Everything the board sends while a probe window is open (deduplicated, max 12 entries).
inline uint32_t probe_until = 0;
inline std::string probe_log;
inline int probe_n = 0;
inline bool probe_active(uint32_t now) { return probe_until != 0 && (int32_t)(probe_until - now) > 0; }
// The board echoed exactly what was sent → it is a WRITE command (value stored in the board).
inline bool probe_echoed(const std::string &sent) { return !sent.empty() && probe_log.find(sent) != std::string::npos; }
inline void note_reply(const std::string &cmd, const std::string &val, uint32_t now) {
  if (!probe_active(now) || probe_n >= 12) return;
  std::string e = "[" + cmd + ":" + val + "]";
  if (probe_log.find(e) != std::string::npos) return;
  if (!probe_log.empty()) probe_log += " ";
  probe_log += e; probe_n++;
}
// Speed command format (same idea as incline; default = original console "[SETSPD:%03d]" in 0.1 km/h).
inline const char *DEFAULT_SPD_TPL = "[SETSPD:%03d]";
inline std::string spd_tpl = DEFAULT_SPD_TPL;
inline int spd_scale = 10;        // command value = round(km/h * spd_scale)
inline bool valid_spd_tpl(const std::string &t) {
  if (t.rfind("[SETSPD:", 0) != 0) return false;
  std::string x = "[SETINC:" + t.substr(8);  // same rules as the incline template
  return valid_tpl(x);
}
// tenths = km/h × 10 (target_speed_goal units)
inline std::string speed_cmd(int tenths) {
  const std::string &t = valid_spd_tpl(spd_tpl) ? spd_tpl : std::string(DEFAULT_SPD_TPL);
  long v = lroundf(tenths * (float) spd_scale / 10.0f);
  char b[32];
  snprintf(b, sizeof(b), t.c_str(), (int) v);
  return b;
}
// Board settings found by the command search (10.10): stored IN the board (survive RESET); the board echoes the value.
// SPDMAX / SPDMIN in 0.1 km/h ("[SPDMAX:180]" = 18.0), INCMAX in whole levels ("[INCMAX:15]").
inline bool raw_active(uint32_t now) { return raw_until != 0 && (int32_t)(raw_until - now) > 0; }
// PSA15H-0200 service menu (learned from the original console, 11.10): the console writes
// [GC3:020] [NS1:0250] [NS3:0020] [WSIZE:183] [SPDMAX:134] [SPDMIN:06] [INCMAX:15]; the board echoes each one back.
// [FADJ] starts the board's own incline auto-calibration; the console then polls [ZT] every 0.2 s and the board answers
// [ZT:pos,000,flag] — pos = incline potentiometer (bottom ≈ 82, top ≈ 245), flag 1 = done.
inline std::string board_acks;          // echoes received since the last "write settings"
inline void note_ack(const std::string &cmd, const std::string &val) {
  static const char *K[] = {"GC3", "NS1", "NS3", "WSIZE", "SPDMAX", "SPDMIN", "INCMAX"};
  for (auto k : K) if (cmd == k) { if (!board_acks.empty()) board_acks += " "; board_acks += cmd + ":" + val; return; }
}
inline bool cal_active = false;
inline int cur_tenths = -1;         // real deck position from [ZT] (our level × 10), -1 unknown
inline int zero = 10;               // [ZT] reading at level 0 (deck on the calibration bottom); learned after each auto-calibration
inline bool want_zero = false;      // calibration just finished: the next low [ZT] outside calibration becomes the zero
// Deck position for "back to 0 after a stop" (11.10). Source: 0 auto ([ZT] if the board answers it, else the [SETINC] echo),
// 1 always [ZT], 2 always the echo. The query is configurable for boards that answer [ZT] under another name (empty = no polling).
inline uint32_t zt_at = 0;          // millis of the last parsed position reply
inline int pos_src = 0;
inline std::string pos_query = "[ZT]";
inline int park_tenths = 8;         // where this board parks at target 0, our tenths above zero; learned after each return
inline bool returning = false;      // motor stopped, deck being brought to 0
inline bool use_zt(uint32_t now) { return pos_src == 1 || (pos_src == 0 && zt_at != 0 && now - zt_at < 5000); }
inline std::string pos_query_name() {   // "[ZT]" -> "ZT", "[POS:1]" -> "POS"
  auto s = pos_query.find('['); if (s == std::string::npos) return "";
  auto e = pos_query.find_first_of(":]", s + 1); return e == std::string::npos ? "" : pos_query.substr(s + 1, e - s - 1);
}
inline uint32_t cal_start = 0;
inline int zt_pos = -1, zt_min = -1, zt_max = -1, zt_flag = 0;
// "240,000,0" → pos 240, flag 0. Returns false if not parseable.
inline bool parse_zt(const std::string &v) {
  int a = 0, b = 0, c = 0;
  if (sscanf(v.c_str(), "%d,%d,%d", &a, &b, &c) < 1) return false;
  zt_pos = a; zt_flag = c; zt_at = esphome::millis();
  if (!cal_active && want_zero && c != 1 && a < 40) { zero = a; want_zero = false; }
  if (!cal_active && a >= 5) cur_tenths = a > zero ? a - zero : 0;   // outside calibration: zero + our level × 10
  if (cal_active && c == 0) { if (zt_min < 0 || a < zt_min) zt_min = a; if (zt_max < 0 || a > zt_max) zt_max = a; }
  return true;
}
// While calibrating only the calibration traffic goes out (the console sends nothing else then).
inline bool svc_busy = false;   // paced service sequence running
inline bool blocked_by_cal(const std::string &cmd) {
  if (!cal_active && !svc_busy) return false;
  static const char *OK[] = {"[ZT", "[FADJ", "[RESET", "[GC3:", "[NS1:", "[NS3:", "[WSIZE:", "[SPDMAX:", "[SPDMIN:", "[INCMAX:"};
  for (auto k : OK) if (cmd.rfind(k, 0) == 0) return false;
  return true;
}

// Listen mode (10.10): the ESP sends NOTHING on the UART and logs every frame it hears (e.g. the original console
// on the same line) — consecutive repeats collapse into "×N". Used to learn the console's service/calibration commands.
inline bool listen = false;
inline uint32_t listen_ver = 0, listen_t0 = 0;
struct ListenLine { std::string f; uint32_t n; uint32_t t; };
inline std::vector<ListenLine> listen_log;
inline void listen_note(const std::string &frame, uint32_t now) {
  if (!listen || frame.empty()) return;
  if (!listen_log.empty() && listen_log.back().f == frame) { listen_log.back().n++; listen_ver++; return; }
  if (listen_log.size() >= 80) listen_log.erase(listen_log.begin());
  listen_log.push_back({frame, 1, now - listen_t0});
  listen_ver++;
}
inline std::string listen_text() {
  std::string o; char b[24];
  for (auto &l : listen_log) {
    snprintf(b, sizeof(b), "%lu.%lu ", (unsigned long) (l.t / 1000), (unsigned long) (l.t % 1000 / 100));
    o += b; o += l.f;
    if (l.n > 1) { snprintf(b, sizeof(b), " x%lu", (unsigned long) l.n); o += b; }
    o += "\n";
  }
  return o;
}
// Finer incline on boards that only take whole levels (PSA15H, 11.10): write INCMAX = k × our max (e.g. 60 = 4 × 15)
// and recalibrate — each board level becomes 1/k of ours. The board stops ~0.6 of OUR level early when lowering and
// ignores smaller descents, so a descent is sent `comp` lower and released once the deck has arrived (position from [ZT]).
inline float board_div = 1.0f;      // board levels per our level (INCMAX / our max)
inline int comp_tenths = 0;         // descent compensation, 0.1 of our level (0 = off)
inline int last_t = -1, comp_last_pos = -1;
inline bool comp_on = false;
inline uint32_t comp_since = 0;
inline int map_target(int t, uint32_t now) {
  if (t != last_t) {
    last_t = t;
    comp_on = comp_tenths > 0 && t > 0 && cur_tenths >= 0 && t < cur_tenths - 1;
    comp_since = now; comp_last_pos = cur_tenths;
  }
  if (comp_on) {
    if (cur_tenths >= 0 && cur_tenths <= t + 1) comp_on = false;                     // arrived
    else if (cur_tenths != comp_last_pos) { comp_last_pos = cur_tenths; comp_since = now; }
    else if (now - comp_since > 4000) comp_on = false;                                // stuck → give up
  }
  int v = comp_on ? t - comp_tenths : t;
  return v < 0 ? 0 : v;
}
// tenths = target_incline_goal (our level × 10) → board command (whole board levels × 10 before the template scale)
inline std::string incline_cmd_at(int tenths, uint32_t now) {
  int m = map_target(tenths, now);
  // divider 1 = legacy: tenths as before; otherwise whole board levels (the board drops the fraction anyway)
  int board10 = board_div == 1.0f ? m : (int) lroundf(m / 10.0f * board_div) * 10;
  const std::string &t = valid_tpl(tpl) ? tpl : std::string(DEFAULT_TPL);
  long v = lroundf(board10 * (float) scale / 10.0f);
  char b[32];
  snprintf(b, sizeof(b), t.c_str(), (int) v);
  return b;
}
inline std::string incline_cmd(int tenths) { return incline_cmd_at(tenths, esphome::millis()); }
}  // namespace treadmill_motor
