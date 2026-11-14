// Ignore Zephyr. It doesn't have any encryption library.
#if defined(USE_ESP32) || defined(USE_ARDUINO) || defined(USE_HOST)

#include "dsmr.h"
#include "esphome/core/log.h"
#include <dsmr_parser/util.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome::dsmr {

static constexpr auto &TAG = "dsmr";
static constexpr uint32_t GAS_MBUS_PREFERENCE_KEY = 0x44534D42;

static void log_callback(dsmr_parser::LogLevel level, const char *fmt, va_list args) {
  std::array<char, 256> buf;
  vsnprintf(buf.data(), buf.size(), fmt, args);
  switch (level) {
    case dsmr_parser::LogLevel::ERROR:
      ESP_LOGE(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::WARNING:
      ESP_LOGW(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::INFO:
      ESP_LOGI(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::VERBOSE:
      ESP_LOGV(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::VERY_VERBOSE:
      ESP_LOGVV(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::DEBUG:
      ESP_LOGD(TAG, "%s", buf.data());
      break;
  }
}

void Dsmr::setup() {
  dsmr_parser::Logger::set_log_function(log_callback);
  this->gas_mbus_preference_ = global_preferences->make_preference<uint8_t>(GAS_MBUS_PREFERENCE_KEY, true);
  uint8_t stored_mbus_id = 1;
  if (this->gas_mbus_preference_.load(&stored_mbus_id) && (stored_mbus_id == 1 || stored_mbus_id == 2)) {
    this->gas_mbus_id_ = stored_mbus_id;
  }
  ESP_LOGI(TAG, "Gas MBUS selection: %u (automatic)", this->gas_mbus_id_);
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
  }
}

void Dsmr::loop() {
  if (!this->ready_to_request_data_()) {
    return;
  }

  if (this->encryption_enabled_) {
    this->receive_encrypted_telegram_();
  } else {
    this->receive_telegram_();
  }
}

bool Dsmr::ready_to_request_data_() {
  if (!this->requesting_data_ && this->request_interval_reached_()) {
    this->start_requesting_data_();
  }
  return this->requesting_data_;
}

bool Dsmr::request_interval_reached_() const {
  if (this->last_request_time_ == 0) {
    return true;
  }
  return millis() - this->last_request_time_ > this->request_interval_;
}

void Dsmr::start_requesting_data_() {
  if (this->requesting_data_) {
    return;
  }

  ESP_LOGV(TAG, "Start reading data from P1 port");
  this->flush_rx_buffer_();

  if (this->request_pin_ != nullptr) {
    ESP_LOGV(TAG, "Set request pin to 1");
    this->request_pin_->digital_write(true);
  }

  this->requesting_data_ = true;
  this->last_request_time_ = millis();
}

void Dsmr::stop_requesting_data_() {
  if (!this->requesting_data_) {
    return;
  }

  ESP_LOGV(TAG, "Stop reading data from P1 port");
  if (this->request_pin_ != nullptr) {
    ESP_LOGV(TAG, "Set request pin to 0");
    this->request_pin_->digital_write(false);
  }
  this->requesting_data_ = false;
}

void Dsmr::flush_rx_buffer_() {
  ESP_LOGV(TAG, "Flush UART RX buffer");
  while (!this->uart_read_chunk_().empty()) {
  }
}

void Dsmr::receive_telegram_() {
  for (auto data = this->uart_read_chunk_(); !data.empty(); data = this->uart_read_chunk_()) {
    for (uint8_t byte : data) {
      const auto telegram = this->packet_accumulator_.process_byte(byte);
      if (!telegram) {  // No full packet received yet
        continue;
      }
      if (this->parse_telegram_(telegram.value())) {
        return;
      }
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  for (auto data = this->uart_read_chunk_(); !data.empty(); data = this->uart_read_chunk_()) {
    for (uint8_t byte : data) {
      if (this->buffer_pos_ >= this->buffer_.size()) {  // Reset buffer if overflow
        ESP_LOGW(TAG, "Encrypted buffer overflow, resetting");
        this->buffer_pos_ = 0;
      }

      this->buffer_[this->buffer_pos_] = byte;
      this->buffer_pos_++;
    }
    this->last_read_time_ = millis();
  }

  // Detect inter-frame delay. If no byte is received for more than receive_timeout, then the packet is complete.
  if (millis() - this->last_read_time_ > this->receive_timeout_ && this->buffer_pos_ > 0) {
    ESP_LOGV(TAG, "Encrypted telegram received (%zu bytes)", this->buffer_pos_);

    const auto telegram = this->dlms_decryptor_.decrypt_inplace({this->buffer_.data(), this->buffer_pos_});

    // Reset buffer position for the next packet
    this->buffer_pos_ = 0;
    this->last_read_time_ = 0;

    if (!telegram) {  // decryption failed
      return;
    }

    // Parse and publish the telegram
    this->parse_telegram_(telegram.value());
  }
}

bool Dsmr::parse_telegram_(const dsmr_parser::DsmrUnencryptedTelegram &telegram) {
  this->stop_requesting_data_();

  ESP_LOGV(TAG, "Trying to parse telegram (%zu bytes)", telegram.full_content().size());
  ESP_LOGVV(TAG, "Telegram content:\n %.*s", static_cast<int>(telegram.full_content().size()),
            telegram.full_content().data());

  MyData data;
  if (const bool res = dsmr_parser::DsmrParser::parse(data, telegram); !res) {
    ESP_LOGE(TAG, "Failed to parse telegram");
    return false;
  }

  this->status_clear_warning();
  this->publish_sensors(data);
  this->publish_selected_gas_(telegram.full_content());

  // Publish the telegram, after publishing the sensors so it can also trigger action based on latest values
  if (this->s_telegram_ != nullptr) {
    this->s_telegram_->publish_state(telegram.full_content().data(), telegram.full_content().size());
  }
  return true;
}

bool Dsmr::parse_gas_reading_(std::string_view telegram, uint8_t mbus_id, float *value) const {
  if (mbus_id != 1 && mbus_id != 2) {
    return false;
  }

  char obis[16];
  std::snprintf(obis, sizeof(obis), "0-%u:24.2.1(", mbus_id);
  const auto start = telegram.find(obis);
  if (start == std::string_view::npos) {
    return false;
  }

  const auto timestamp_end = telegram.find(')', start + std::strlen(obis));
  if (timestamp_end == std::string_view::npos) {
    return false;
  }
  const auto value_start = timestamp_end + 1;
  if (value_start >= telegram.size() || telegram[value_start] != '(') {
    return false;
  }

  const auto value_end = telegram.find(')', value_start + 1);
  if (value_end == std::string_view::npos || value_end <= value_start + 1) {
    return false;
  }

  char value_text[32];
  const auto value_length = value_end - value_start - 1;
  if (value_length >= sizeof(value_text)) {
    return false;
  }
  std::memcpy(value_text, telegram.data() + value_start + 1, value_length);
  value_text[value_length] = '\0';

  char *end = nullptr;
  const float parsed = std::strtof(value_text, &end);
  if (end == value_text || !std::isfinite(parsed) || parsed < 0.0f) {
    return false;
  }
  *value = parsed;
  return true;
}

void Dsmr::publish_selected_gas_(std::string_view telegram) {
  if (this->s_gas_delivered_ == nullptr) {
    return;
  }

  float mbus1_value = 0.0f;
  float mbus2_value = 0.0f;
  const bool mbus1_present = this->parse_gas_reading_(telegram, 1, &mbus1_value);
  const bool mbus2_present = this->parse_gas_reading_(telegram, 2, &mbus2_value);
  const bool selected_present = this->gas_mbus_id_ == 1 ? mbus1_present : mbus2_present;
  const bool alternate_present = this->gas_mbus_id_ == 1 ? mbus2_present : mbus1_present;

  if (!selected_present && alternate_present) {
    this->gas_mbus_id_ = this->gas_mbus_id_ == 1 ? 2 : 1;
    this->gas_mbus_preference_.save(&this->gas_mbus_id_);
    global_preferences->sync();
    ESP_LOGI(TAG, "Gas value found on MBUS %u; saved automatic selection", this->gas_mbus_id_);
  }

  if (this->gas_mbus_id_ == 1 && mbus1_present) {
    this->s_gas_delivered_->publish_state(mbus1_value);
  } else if (this->gas_mbus_id_ == 2 && mbus2_present) {
    this->s_gas_delivered_->publish_state(mbus2_value);
  }
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG,
                "DSMR:\n"
                "  Max telegram length: %zu\n"
                "  Receive timeout: %.1fs",
                this->buffer_.size(), this->receive_timeout_ / 1e3f);
  if (this->request_pin_ != nullptr) {
    LOG_PIN("  Request Pin: ", this->request_pin_);
  }
  if (this->request_interval_ > 0) {
    ESP_LOGCONFIG(TAG, "  Request Interval: %.1fs", this->request_interval_ / 1e3f);
  }

#define DSMR_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s_##s##_);
  DSMR_SENSOR_LIST(DSMR_LOG_SENSOR, )

#define DSMR_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s_##s##_);
  DSMR_TEXT_SENSOR_LIST(DSMR_LOG_TEXT_SENSOR, )
}

void Dsmr::set_decryption_key_(const char *decryption_key) {
  if (decryption_key == nullptr || decryption_key[0] == '\0') {
    this->encryption_enabled_ = false;
    return;
  }

  auto key = dsmr_parser::Aes128GcmDecryptionKey::from_hex(decryption_key);
  if (!key) {
    ESP_LOGE(TAG, "Error, decryption key has incorrect format");
    this->encryption_enabled_ = false;
    return;
  }

  ESP_LOGI(TAG, "Decryption key is set");

  this->gcm_decryptor_.set_encryption_key(key.value());
  this->encryption_enabled_ = true;
}

std::span<uint8_t> Dsmr::uart_read_chunk_() {
  const auto avail = this->available();
  if (avail == 0) {
    return {};
  }
  size_t to_read = std::min(avail, uart_chunk_reading_buf_.size());
  if (!this->read_array(uart_chunk_reading_buf_.data(), to_read)) {
    return {};
  }
  return {uart_chunk_reading_buf_.data(), to_read};
}

}  // namespace esphome::dsmr

#endif
