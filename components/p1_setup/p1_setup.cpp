#include "p1_setup_internal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cstdio>

namespace esphome::p1_setup {

namespace {

bool parse_timestamp(const std::string &timestamp, uint32_t *day_key, uint8_t *hour) {
  if (timestamp.size() < 10)
    return false;
  for (size_t i = 0; i < 10; i++) {
    if (timestamp[i] < '0' || timestamp[i] > '9')
      return false;
  }
  const uint16_t year = 2000 + (timestamp[0] - '0') * 10 + timestamp[1] - '0';
  const uint8_t month = (timestamp[2] - '0') * 10 + timestamp[3] - '0';
  const uint8_t day = (timestamp[4] - '0') * 10 + timestamp[5] - '0';
  *hour = (timestamp[6] - '0') * 10 + timestamp[7] - '0';
  if (month < 1 || month > 12 || day < 1 || day > 31 || *hour > 23)
    return false;
  *day_key = static_cast<uint32_t>(year) * 10000UL + month * 100UL + day;
  return true;
}

uint32_t previous_day(uint32_t day_key) {
  uint16_t year = day_key / 10000UL;
  uint8_t month = (day_key / 100UL) % 100;
  uint8_t day = day_key % 100;
  if (day > 1) {
    day--;
  } else {
    if (month == 1) {
      year--;
      month = 12;
    } else {
      month--;
    }
    const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    const uint8_t days_in_month[] = {31, static_cast<uint8_t>(leap ? 29 : 28), 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
    day = days_in_month[month - 1];
  }
  return static_cast<uint32_t>(year) * 10000UL + month * 100UL + day;
}

sensor::Sensor *find_sensor(const char *name) {
  for (auto *sensor : App.get_sensors()) {
    if (sensor != nullptr && !strcmp(sensor->get_name().c_str(), name))
      return sensor;
  }
  return nullptr;
}

text_sensor::TextSensor *find_text_sensor(const char *name) {
  for (auto *sensor : App.get_text_sensors()) {
    if (sensor != nullptr && !strcmp(sensor->get_name().c_str(), name))
      return sensor;
  }
  return nullptr;
}

}  // namespace

void P1Setup::setup() {
  this->login_realm_ready_ = this->rotate_login_realm_();
  if (!this->login_realm_ready_)
    ESP_LOGE("p1_setup", "Could not initialize the setup login realm");

  this->settings_preference_ = global_preferences->make_preference<SetupSettings>(SETTINGS_PREFERENCE_KEY, true);
  if (!this->settings_preference_.load(&this->settings_) || this->settings_.magic != SETTINGS_MAGIC) {
    memset(&this->settings_, 0, sizeof(this->settings_));
    this->settings_.magic = SETTINGS_MAGIC;
    this->settings_.mqtt_port = 1883;
  }
  this->history_preference_ = global_preferences->make_preference<HistorySettings>(HISTORY_PREFERENCE_KEY, true);
  if (!this->history_preference_.load(&this->history_) || this->history_.magic != HISTORY_MAGIC) {
    memset(&this->history_, 0, sizeof(this->history_));
    this->history_.magic = HISTORY_MAGIC;
  }
  this->setup_password_preference_ =
      global_preferences->make_preference<SetupPassword>(SETUP_PASSWORD_PREFERENCE_KEY, true);
  SetupPassword stored_password{};
  if (this->setup_password_preference_.load(&stored_password) && stored_password.magic == SETUP_PASSWORD_MAGIC) {
    this->setup_password_ = stored_password.password;
  } else {
    this->setup_password_ = this->initial_setup_password_;
  }
  if (wifi::global_wifi_component != nullptr) {
    wifi::WiFiAP access_point = wifi::global_wifi_component->get_ap();
    access_point.set_password(this->setup_password_);
    wifi::global_wifi_component->set_ap(access_point);
  }

  this->mqtt_enabled_preference_ = global_preferences->make_preference<bool>(MQTT_ENABLED_PREFERENCE_KEY, true);
  if (!this->mqtt_enabled_preference_.load(&this->mqtt_enabled_)) {
    this->mqtt_enabled_ = this->settings_.mqtt_broker[0] != '\0';
    this->mqtt_enabled_preference_.save(&this->mqtt_enabled_);
    global_preferences->sync();
  }

  if (this->settings_.wifi_ssid[0] != '\0' && wifi::global_wifi_component != nullptr) {
    wifi::WiFiAP station;
    station.set_ssid(this->settings_.wifi_ssid);
    station.set_password(this->settings_.wifi_password);
    if (this->settings_.ip_mode == 1) {
      wifi::ManualIP manual_ip{
          .static_ip = network::IPAddress(this->settings_.static_ip[0], this->settings_.static_ip[1],
                                          this->settings_.static_ip[2], this->settings_.static_ip[3]),
          .gateway = network::IPAddress(this->settings_.gateway[0], this->settings_.gateway[1],
                                        this->settings_.gateway[2], this->settings_.gateway[3]),
          .subnet = network::IPAddress(this->settings_.subnet[0], this->settings_.subnet[1],
                                       this->settings_.subnet[2], this->settings_.subnet[3]),
          .dns1 = network::IPAddress(this->settings_.dns[0], this->settings_.dns[1], this->settings_.dns[2],
                                     this->settings_.dns[3]),
          .dns2 = network::IPAddress(),
      };
      station.set_manual_ip(optional<wifi::ManualIP>(manual_ip));
    }
    wifi::global_wifi_component->set_sta(station);
  }

  if (this->mqtt_client_ != nullptr) {
    this->mqtt_client_->set_broker_address(this->settings_.mqtt_broker);
    this->mqtt_client_->set_broker_port(this->settings_.mqtt_port == 0 ? 1883 : this->settings_.mqtt_port);
    this->mqtt_client_->set_username(this->settings_.mqtt_username);
    this->mqtt_client_->set_password(this->settings_.mqtt_password);
    this->mqtt_client_->set_enable_on_boot(this->mqtt_enabled_ && this->settings_.mqtt_broker[0] != '\0');
    this->mqtt_client_->set_reboot_timeout(0);
  }

  this->web_server_base_ = web_server_base::global_web_server_base;
  if (this->web_server_base_ != nullptr) {
    this->web_server_base_->add_handler_without_auth(&this->captive_probe_);
    this->web_server_base_->add_handler_without_auth(&this->ap_setup_);
    this->web_server_base_->add_handler_without_auth(this);
  } else {
    ESP_LOGE("p1_setup", "Shared web server is unavailable");
  }
}

void P1Setup::loop() {
  this->update_history_();
  const bool ap_active = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active();
  if (ap_active && !this->dns_active_) {
    this->dns_server_.setErrorReplyCode(DNSReplyCode::NoError);
    this->dns_active_ = this->dns_server_.start(53, "*", WiFi.softAPIP());
  } else if (!ap_active && this->dns_active_) {
    this->dns_server_.stop();
    this->dns_active_ = false;
  }
  if (this->dns_active_)
    this->dns_server_.processNextRequest();

  if (this->reboot_at_ != 0 && static_cast<int32_t>(millis() - this->reboot_at_) >= 0) {
    this->reboot_at_ = 0;
    App.reboot();
  }
}

void P1Setup::save_history_() {
  this->history_preference_.save(&this->history_);
  global_preferences->sync();
}

void P1Setup::update_history_() {
  if (this->last_history_sample_ != 0 && millis() - this->last_history_sample_ < 10000UL)
    return;
  this->last_history_sample_ = millis();

  auto *timestamp_sensor = find_text_sensor("DSMR Timestamp");
  auto *power_consumed = find_sensor("Power Consumed");
  auto *power_returned = find_sensor("Power Returned");
  auto *gas_consumed = find_sensor("Gas Consumed");
  if (timestamp_sensor == nullptr || !timestamp_sensor->has_state() || power_consumed == nullptr ||
      power_returned == nullptr || gas_consumed == nullptr || !power_consumed->has_state() ||
      !power_returned->has_state() || !gas_consumed->has_state())
    return;

  uint32_t day_key;
  uint8_t hour;
  if (!parse_timestamp(timestamp_sensor->get_state(), &day_key, &hour))
    return;
  const uint32_t hour_key = day_key * 24UL + hour;
  const size_t index = hour_key % 72UL;
  auto &bucket = this->history_.buckets[index];
  bool new_bucket = !bucket.valid || bucket.hour_key != hour_key;
  if (new_bucket) {
    memset(&bucket, 0, sizeof(bucket));
    bucket.hour_key = hour_key;
    bucket.valid = 1;
  }
  bucket.power_consumed_sum += power_consumed->get_state();
  bucket.power_returned_sum += power_returned->get_state();
  bucket.gas_consumed_sum += gas_consumed->get_state();
  if (bucket.sample_count < UINT16_MAX)
    bucket.sample_count++;
  if (new_bucket)
    this->save_history_();
}

void P1Setup::append_history_json_(String &json, uint8_t day_offset) const {
  uint32_t current_day_key = 0;
  uint32_t latest_hour_key = 0;
  for (const auto &bucket : this->history_.buckets) {
    if (bucket.valid && (current_day_key == 0 || bucket.hour_key > latest_hour_key)) {
      latest_hour_key = bucket.hour_key;
      current_day_key = bucket.hour_key / 24UL;
    }
  }
  uint32_t requested_day = current_day_key;
  for (uint8_t i = 0; i < day_offset && requested_day != 0; i++)
    requested_day = previous_day(requested_day);

  json += F(",\"history\":{\"day_offset\":");
  json += day_offset;
  json += F(",\"day_key\":");
  json += requested_day;
  json += F(",\"hours\":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23],\"power_consumed\":[");
  for (uint8_t hour = 0; hour < 24; hour++) {
    if (hour > 0)
      json += ',';
    const uint32_t hour_key = requested_day * 24UL + hour;
    const auto &bucket = this->history_.buckets[hour_key % 72UL];
    if (!bucket.valid || bucket.hour_key != hour_key || bucket.sample_count == 0) {
      json += F("null");
    } else {
      json += String(bucket.power_consumed_sum / bucket.sample_count, 6);
    }
  }
  json += F("],\"power_returned\":[");
  for (uint8_t hour = 0; hour < 24; hour++) {
    if (hour > 0)
      json += ',';
    const uint32_t hour_key = requested_day * 24UL + hour;
    const auto &bucket = this->history_.buckets[hour_key % 72UL];
    if (!bucket.valid || bucket.hour_key != hour_key || bucket.sample_count == 0)
      json += F("null");
    else
      json += String(bucket.power_returned_sum / bucket.sample_count, 6);
  }
  json += F("],\"gas_consumed\":[");
  for (uint8_t hour = 0; hour < 24; hour++) {
    if (hour > 0)
      json += ',';
    const uint32_t hour_key = requested_day * 24UL + hour;
    const auto &bucket = this->history_.buckets[hour_key % 72UL];
    if (!bucket.valid || bucket.hour_key != hour_key || bucket.sample_count == 0)
      json += F("null");
    else
      json += String(bucket.gas_consumed_sum / bucket.sample_count, 6);
  }
  json += F("]}");
}

}  // namespace esphome::p1_setup
