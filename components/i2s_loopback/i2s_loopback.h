#pragma once

#include "esphome/core/component.h"
#include "driver/i2s_std.h"
#include <cmath>

namespace i2s_loopback {

class I2SLoopback : public esphome::Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void loop() override {}

  void start();
  void stop();
  bool is_running() { return this->running_; }
  void set_r_atten(float db) { this->r_atten_ = powf(10.0f, -db / 20.0f); }
  void set_l_atten(float db) { this->l_atten_ = powf(10.0f, -db / 20.0f); }

  // Health/diagnostic counters (added 2026-05-01 for buffer sizing analysis)
  uint32_t get_reads_ok() { return this->reads_ok_; }
  uint32_t get_reads_fail() { return this->reads_fail_; }
  uint32_t get_max_proc_us() { return this->max_proc_us_; }
  void reset_max_proc_us() { this->max_proc_us_ = 0; }

 protected:
  bool running_{false};
  float r_atten_{1.0f};
  float l_atten_{1.0f};
  i2s_chan_handle_t tx_handle_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};
  TaskHandle_t task_handle_{nullptr};
  volatile uint32_t reads_ok_{0};
  volatile uint32_t reads_fail_{0};
  volatile uint32_t max_proc_us_{0};
  static void loopback_task(void *arg);
};

}  // namespace i2s_loopback
