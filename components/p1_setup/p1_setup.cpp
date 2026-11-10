#include "p1_setup_internal.h"

namespace esphome::p1_setup {

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

}  // namespace esphome::p1_setup
