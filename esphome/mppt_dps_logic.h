#pragma once
/**
 * Lógica MPPT OSPController — somente DPS (Modbus).
 * Chamada a partir de lambdas ESPHome; usa id(...) dos componentes do YAML.
 */
#include "esphome.h"
#include <vector>
#include <algorithm>

namespace osp_mppt {

struct SweepPoint {
  float v, i, input;
  bool collapsed;
  float power() const { return v * i; }
};

// Estados (solar.h States)
enum State : int {
  ST_OFF = 0,
  ST_MPPT = 1,
  ST_SWEEPING = 2,
  ST_FULL_CV = 3,
  ST_CAPPED = 4,
  ST_COLLAPSEMODE = 5,
  ST_ERROR = 6,
};

inline float current_scale() { return id(dps_current_scale); }

inline bool read_dps_values(float &limit_v, float &limit_c, float &out_v, float &out_c, float &in_v, bool &out_en) {
  limit_v = id(dps_limit_volt).state;
  limit_c = id(dps_limit_curr).state;
  out_v = id(dps_out_volt).state;
  out_c = id(dps_out_curr).state;
  in_v = id(mppt_in_volt);
  out_en = id(dps_out_en).state;
  return !isnan(limit_v) && !isnan(limit_c) && !isnan(out_v) && !isnan(out_c) && !isnan(in_v);
}

inline bool is_cv(float limit_v, float out_v) {
  if (limit_v < 0.1f) return false;
  return ((limit_v - out_v) / limit_v) < 0.004f;
}

inline bool is_cc(float limit_c, float out_c) {
  if (id(dps_is_5020)) {
    if (limit_c < 0.01f) return false;
    return ((limit_c - out_c) / limit_c) < 0.02f;
  }
  return id(dps_cc_flag).state;
}

inline bool is_collapsed_psu(bool out_en, float limit_v, float out_v, float limit_c, float out_c) {
  if (!out_en) return false;
  return !is_cv(limit_v, out_v) && !is_cc(limit_c, out_c);
}

inline bool has_collapsed(float in_v, float out_v, bool out_en, float limit_v, float limit_c, float out_c) {
  if (!out_en) return false;
  bool clps = is_collapsed_psu(out_en, limit_v, out_v, limit_c, out_c);
  if (clps) return true;
  bool simple = in_v < (out_v * 1.11f);
  float collapse_pct = (in_v - out_v) / (out_v > 0.01f ? out_v : 0.01f);
  return (simple && clps) || (collapse_pct < 0.05f && clps);
}

inline void apply_current(float amps) {
  float cap = id(mppt_current_cap).state;
  if (amps < 0.01f) amps = 0.01f;
  if (amps > cap) amps = cap;
  id(dps_set_curr).make_call().set_value(amps).perform();
  float oc = id(dps_out_curr).state;
  if (!isnan(oc))
    id(mppt_curr_filt) = id(mppt_curr_filt) - 0.1f * (id(mppt_curr_filt) - oc);
}

inline void set_state(int st) {
  if (id(mppt_state) != st) {
    ESP_LOGI("mppt", "state %d -> %d", id(mppt_state), st);
    id(mppt_state) = st;
  }
}

inline std::vector<SweepPoint> &sweep_buf() {
  static std::vector<SweepPoint> pts;
  return pts;
}

inline void restore_from_collapse(float restore_current) {
  apply_current(0.01f);
  uint32_t start = millis();
  while ((millis() - start) < 8000) {
    id(dps_in_volt).update();
    float in_v = id(mppt_in_volt);
    if (in_v >= id(mppt_off_threshold)) break;
    delay(25);
  }
  float in_v = id(mppt_in_volt);
  if (id(mppt_off_threshold) >= 1000.0f) {
    id(mppt_off_threshold) = 0.992f * in_v;
    ESP_LOGI("mppt", "off_threshold = %.2f V", (float) id(mppt_off_threshold));
  }
  ESP_LOGI("mppt", "restore %.2f A after Vin=%.2f", restore_current, in_v);
  apply_current(restore_current);
}

inline void start_sweep() {
  if (id(mppt_state) == ST_ERROR) return;
  sweep_buf().clear();
  float lc = id(dps_limit_curr).state;
  if (isnan(lc)) lc = 0.1f;
  apply_current(id(mppt_curr_filt) * 0.90f);
  if (has_collapsed(id(mppt_in_volt), id(dps_out_volt).state, id(dps_out_en).state,
                    id(dps_limit_volt).state, lc, id(dps_out_curr).state))
    restore_from_collapse(id(mppt_curr_filt) * 0.75f);
  set_state(ST_SWEEPING);
  id(mppt_last_autosweep) = millis();
  if (!id(dps_out_en).state)
    id(dps_output_switch).turn_on();
}

inline void sweep_step() {
  float lv, lc, ov, oc, inv;
  bool out_en;
  if (!read_dps_values(lv, lc, ov, oc, inv, out_en)) return;
  if (!out_en) {
    set_state(ST_MPPT);
    return;
  }

  bool collapsed = has_collapsed(inv, ov, out_en, lv, lc, oc);
  sweep_buf().push_back({ov, oc, inv, collapsed});

  int collapsed_pts = 0, ok_pts = 0, max_i = 0;
  float max_p = 0;
  for (size_t i = 0; i < sweep_buf().size(); i++) {
    if (sweep_buf()[i].collapsed) collapsed_pts++;
    else {
      ok_pts++;
      float p = sweep_buf()[i].power();
      if (p > max_p) {
        max_p = p;
        max_i = (int) i;
      }
    }
  }

  if (collapsed && collapsed_pts >= 2) {
    if (ok_pts == 0) {
      restore_from_collapse(id(mppt_curr_filt) * 0.5f);
      set_state(ST_MPPT);
      sweep_buf().clear();
      return;
    }
    int use_i = max_i > 2 ? max_i - 2 : 0;
    auto &best = sweep_buf()[use_i];
    auto &last = sweep_buf().back();
    if (best.power() < last.power()) {
      set_state(ST_COLLAPSEMODE);
      apply_current(id(mppt_current_cap).state > 0 ? id(mppt_current_cap).state : 10.0f);
      id(mppt_setpoint).publish_state(last.input);
      uint32_t autosweep = (uint32_t) (id(mppt_autosweep).state * 1000.0f);
      id(mppt_next_autosweep) = millis() + autosweep / 3;
    } else {
      float restore = best.i * 0.98f;
      id(mppt_setpoint).publish_state(best.input);
      set_state(ST_MPPT);
      restore_from_collapse(restore);
    }
    sweep_buf().clear();
    id(mppt_next_adjust_ms) = millis() + 1000;
    return;
  }

  float cap = id(mppt_current_cap).state;
  if (lc >= cap) {
    id(mppt_setpoint).publish_state(inv - id(mppt_pgain).state * 4.0f);
    set_state(ST_MPPT);
    sweep_buf().clear();
    apply_current(cap);
    return;
  }
  if (is_cv(lv, ov)) {
    set_state(ST_FULL_CV);
    sweep_buf().clear();
    ESP_LOGI("mppt", "sweep done: CV");
    return;
  }
  apply_current(std::min(lc + inv * 0.001f, cap + 0.001f));
}

inline float measure_desired_current() {
  if (id(mppt_state) == ST_SWEEPING) {
    sweep_step();
    return id(dps_limit_curr).state;
  }

  float sp = id(mppt_setpoint).state;
  if (sp <= 0 || !id(dps_out_en).state) return id(dps_limit_curr).state;

  float err = id(mppt_in_volt) - sp;
  float pg = id(mppt_pgain).state;
  float ramp = id(mppt_ramplimit).state;
  float dcurr = err * pg;
  dcurr = std::max(-ramp * 2.0f, std::min(ramp * 2.0f, dcurr));
  if (err > 0.3f || err < -0.2f) {
    if (err < 0.6f && id(mppt_state) == ST_MPPT)
      id(mppt_next_adjust_ms) = millis();
    float lc = id(dps_limit_curr).state;
    if (isnan(lc)) lc = 0;
    float desired = lc + dcurr;
    if (desired > id(mppt_current_cap).state)
      desired = id(mppt_current_cap).state;
    return desired;
  }
  return id(dps_limit_curr).state;
}

inline void update_state() {
  if (!id(mppt_enable).state) {
    set_state(ST_OFF);
    return;
  }
  int st = id(mppt_state);
  if (st == ST_SWEEPING || st == ST_COLLAPSEMODE) return;

  float lv, lc, ov, oc, inv;
  bool out_en;
  if (!read_dps_values(lv, lc, ov, oc, inv, out_en)) {
    set_state(ST_ERROR);
    return;
  }
  if (out_en) {
    if (is_cv(lv, ov)) set_state(ST_FULL_CV);
    else if (lc > id(mppt_current_cap).state * 0.95f) set_state(ST_CAPPED);
    else set_state(ST_MPPT);
  } else {
    set_state(ST_OFF);
  }
}

inline int backoff_period(int base_ms) {
  int b = id(mppt_backoff);
  if (b <= 0) return base_ms;
  return ((b * b + 2) / 2) * base_ms;
}

inline void do_adjust(float desired) {
  if (id(mppt_state) == ST_ERROR) return;
  float sp = id(mppt_setpoint).state;
  if (sp <= 0 || id(mppt_state) == ST_SWEEPING) return;

  float lv, lc, ov, oc, inv;
  bool out_en;
  if (!read_dps_values(lv, lc, ov, oc, inv, out_en)) return;

  if (has_collapsed(inv, ov, out_en, lv, lc, oc) && id(mppt_state) != ST_COLLAPSEMODE) {
    ESP_LOGW("mppt", "collapse Vin=%.2f", inv);
    restore_from_collapse(id(mppt_curr_filt) * 0.95f);
    return;
  }
  if (!out_en) {
    if (inv < ov || ov < 0.1f) {
      id(mppt_backoff) = std::min(id(mppt_backoff) + 1, 8);
      return;
    }
  }
  if (out_en && id(mppt_state) != ST_COLLAPSEMODE)
    apply_current(desired);
  if (id(mppt_backoff) > 0) id(mppt_backoff)--;
}

inline void dps_poll() {
  id(dps_limit_volt).update();
  id(dps_limit_curr).update();
  id(dps_out_volt).update();
  id(dps_out_curr).update();
  id(dps_in_volt).update();
  id(dps_cc_flag).update();
  id(dps_out_en).update();
}

inline void tick_measure() {
  dps_poll();
  float lv, lc, ov, oc, inv;
  bool out_en;
  if (read_dps_values(lv, lc, ov, oc, inv, out_en))
    id(mppt_power).publish_state(ov * oc);
  update_state();
  id(mppt_last_desired) = measure_desired_current();
}

inline void tick_adjust() {
  do_adjust(id(mppt_last_desired));
}

inline void tick_autosweep() {
  float as = id(mppt_autosweep).state;
  if (isnan(as)) return;
  int autosweep_s = (int) as;
  if (autosweep_s <= 0) return;
  if (millis() < id(mppt_next_autosweep)) return;

  int st = id(mppt_state);
  if (st == ST_CAPPED) {
    ESP_LOGI("mppt", "skip autosweep: capped");
  } else if (st == ST_FULL_CV) {
    ESP_LOGI("mppt", "skip autosweep: full_cv");
  } else if (st == ST_MPPT || st == ST_COLLAPSEMODE) {
    ESP_LOGI("mppt", "auto-sweep");
    start_sweep();
  }
  id(mppt_next_autosweep) = millis() + (uint32_t) autosweep_s * 1000;
  id(mppt_last_autosweep) = millis();
}

}  // namespace osp_mppt
