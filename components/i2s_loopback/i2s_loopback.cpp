#include "i2s_loopback.h"
#include "esphome/core/log.h"
#include "esp_timer.h"

namespace i2s_loopback {

static const char *const TAG = "i2s_loopback";

void I2SLoopback::setup() {
  ESP_LOGI(TAG, "Installing I2S driver (full-duplex) for MCLK...");

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 8;
  chan_cfg.dma_frame_num = 256;
  chan_cfg.auto_clear = true;

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, &this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
          .mclk = GPIO_NUM_0,
          .bclk = GPIO_NUM_5,
          .ws = GPIO_NUM_25,
          .dout = GPIO_NUM_26,
          .din = GPIO_NUM_35,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
  std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s init TX failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = i2s_channel_init_std_mode(this->rx_handle_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s init RX failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  ESP_ERROR_CHECK(i2s_channel_enable(this->tx_handle_));
  ESP_ERROR_CHECK(i2s_channel_enable(this->rx_handle_));

  ESP_LOGI(TAG, "I2S active: MCLK=12.29MHz on GPIO0, 48kHz/32bit");
}

void I2SLoopback::start() {
  if (this->running_) {
    ESP_LOGW(TAG, "Already running");
    return;
  }

  ESP_LOGI(TAG, "Starting loopback task...");

  // Flush stale RX data
  uint8_t flush[512];
  size_t flushed;
  for (int i = 0; i < 8; i++) {
    i2s_channel_read(this->rx_handle_, flush, sizeof(flush), &flushed, 10);
  }

  this->running_ = true;

  BaseType_t ret = xTaskCreatePinnedToCore(
      I2SLoopback::loopback_task, "i2s_loop", 8192,
      (void *)this, 5, &this->task_handle_, 1);

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Task create failed");
    this->running_ = false;
    return;
  }

  ESP_LOGI(TAG, "Full-duplex loopback active: 48000Hz stereo 32bit");
}

void I2SLoopback::stop() {
  if (!this->running_) return;

  this->running_ = false;
  vTaskDelay(pdMS_TO_TICKS(200));
  this->task_handle_ = nullptr;

  ESP_LOGI(TAG, "Loopback stopped");
}

void I2SLoopback::loopback_task(void *arg) {
  auto *self = (I2SLoopback *)arg;
  uint8_t buf[1024];
  size_t bytes_read = 0;
  size_t bytes_written = 0;

  ESP_LOGI(TAG, "Loopback task running on core %d", xPortGetCoreID());

  while (self->running_) {
    int64_t t_start = esp_timer_get_time();
    esp_err_t ret = i2s_channel_read(self->rx_handle_, buf, sizeof(buf), &bytes_read, 100);
    if (ret == ESP_OK && bytes_read > 0) {
      float r_att = self->r_atten_;
      float l_att = self->l_atten_;
      if (r_att != 1.0f || l_att != 1.0f) {
        int32_t *samples = (int32_t *)buf;
        int num_samples = bytes_read / 4;
        for (int i = 0; i < num_samples; i += 2) {
          if (l_att != 1.0f) samples[i] = (int32_t)((float)samples[i] * l_att);
          if (r_att != 1.0f) samples[i+1] = (int32_t)((float)samples[i+1] * r_att);
        }
      }

      i2s_channel_write(self->tx_handle_, buf, bytes_read, &bytes_written, 100);

      // Diagnostic counters: success path
      uint32_t dt = (uint32_t)(esp_timer_get_time() - t_start);
      self->reads_ok_++;
      if (dt > self->max_proc_us_) self->max_proc_us_ = dt;
    } else {
      // No data ready (timeout or error) — yield briefly so the IDLE task on
      // this core can run and feed the task watchdog. Without this yield,
      // sustained no-data conditions could starve the scheduler over hours.
      self->reads_fail_++;
      vTaskDelay(1);
    }
  }

  ESP_LOGI(TAG, "Task exiting");
  vTaskDelete(NULL);
}

}  // namespace i2s_loopback
