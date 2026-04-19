#pragma once

#include "esphome/core/component.h"
#include "driver/i2s_std.h"
#include <atomic>

namespace i2s_loopback {

class I2SLoopback : public esphome::Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void loop() override {}

  void start();
  void stop();
  bool is_running() { return this->running_.load(); }

 protected:
  std::atomic<bool> running_{false};
  i2s_chan_handle_t tx_handle_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};
  TaskHandle_t task_handle_{nullptr};
  static void loopback_task(void *arg);
};

}  // namespace i2s_loopback
