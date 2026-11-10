#include "p1_setup_internal.h"

namespace esphome::p1_setup {

void P1Setup::save_setup_password_(AsyncWebServerRequest *request) {
  String origin;
  String host;
  if (!request_header(request, "Origin", origin) || !request_header(request, "Host", host) ||
      origin != String("http://") + host) {
    request->send(403, "text/plain", "Invalid request origin");
    return;
  }

  const String password = request_arg(request, "password");
  if (!valid_setup_password(password)) {
    request->send(400, "text/plain", "Use 8 to 63 printable ASCII characters.");
    return;
  }
  SetupPassword stored{};
  stored.magic = SETUP_PASSWORD_MAGIC;
  if (!copy_value(stored.password, sizeof(stored.password), password)) {
    request->send(400, "text/plain", "Use 8 to 63 printable ASCII characters.");
    return;
  }
  if (!this->setup_password_preference_.save(&stored) || !global_preferences->sync()) {
    request->send(500, "text/plain", "Could not save the setup password to flash");
    return;
  }
  this->setup_password_ = password.c_str();
  request->send(200, "text/plain", "Password saved; rebooting");
  this->schedule_reboot_();
}

void P1Setup::set_mqtt_enabled(bool enabled) {
  this->mqtt_enabled_ = enabled && this->settings_.mqtt_broker[0] != '\0';
  this->mqtt_enabled_preference_.save(&this->mqtt_enabled_);
  global_preferences->sync();
  if (this->mqtt_client_ == nullptr)
    return;

  this->mqtt_client_->set_broker_address(this->settings_.mqtt_broker);
  this->mqtt_client_->set_broker_port(this->settings_.mqtt_port == 0 ? 1883 : this->settings_.mqtt_port);
  this->mqtt_client_->set_username(this->settings_.mqtt_username);
  this->mqtt_client_->set_password(this->settings_.mqtt_password);
  if (this->mqtt_enabled_) {
    this->mqtt_client_->enable();
  } else {
    this->mqtt_client_->disable();
  }
}

void P1Setup::send_settings_(AsyncWebServerRequest *request) {
  String origin;
  String host;
  String fetch_site;
  const bool has_origin = request_header(request, "Origin", origin);
  const bool has_host = request_header(request, "Host", host);
  const bool has_fetch_site = request_header(request, "Sec-Fetch-Site", fetch_site);
  if ((has_origin && (!has_host || origin != String("http://") + host)) ||
      (has_fetch_site && fetch_site == "cross-site") ||
      (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active() &&
       is_softap_client(request) && (!has_host || host != "192.168.4.1"))) {
    request->send(403, "text/plain", "Invalid request origin");
    return;
  }
  String json;
  json.reserve(480);
  json += F("{\"wifi_ssid\":");
  append_json_string(json, this->settings_.wifi_ssid);
  json += F(",\"wifi_password\":");
  append_json_string(json, this->settings_.wifi_password);
  json += F(",\"mqtt_broker\":");
  append_json_string(json, this->settings_.mqtt_broker);
  json += F(",\"mqtt_enabled\":");
  json += this->mqtt_enabled_ ? "true" : "false";
  json += F(",\"mqtt_port\":");
  json += String(this->settings_.mqtt_port == 0 ? 1883 : this->settings_.mqtt_port);
  json += F(",\"mqtt_username\":");
  append_json_string(json, this->settings_.mqtt_username);
  json += F(",\"mqtt_password\":");
  append_json_string(json, this->settings_.mqtt_password);
  json += F(",\"ip_mode\":");
  append_json_string(json, this->settings_.ip_mode == 1 ? "static" : "dhcp");
  json += F(",\"static_ip\":");
  append_json_ip(json, this->settings_.static_ip);
  json += F(",\"gateway\":");
  append_json_ip(json, this->settings_.gateway);
  json += F(",\"subnet\":");
  append_json_ip(json, this->settings_.subnet);
  json += F(",\"dns\":");
  append_json_ip(json, this->settings_.dns);
  json += '}';
  AsyncWebServerResponse *response = begin_text_response(request, 200, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void P1Setup::save_settings_(AsyncWebServerRequest *request) {
  SetupSettings next = this->settings_;
  next.magic = SETTINGS_MAGIC;
  next.mqtt_port = 1883;
  next.ip_mode = 0;

  const String ssid = request_arg(request, "wifi_ssid");
  const String wifi_password = request_arg(request, "wifi_password");
  const String broker = request_arg(request, "mqtt_broker");
  const String port_text = request_arg(request, "mqtt_port");
  const String username = request_arg(request, "mqtt_username");
  const String mqtt_password = request_arg(request, "mqtt_password");
  const bool mqtt_enabled = request_arg(request, "mqtt_enabled") == "on";
  if (ssid.isEmpty()) {
    send_field_error(request, "wifi_ssid", "Enter a Wi-Fi network name.");
    return;
  }
  if (!copy_value(next.wifi_ssid, sizeof(next.wifi_ssid), ssid)) {
    send_field_error(request, "wifi_ssid", "Wi-Fi network name must be 32 characters or fewer.");
    return;
  }
  if (!copy_value(next.wifi_password, sizeof(next.wifi_password), wifi_password)) {
    send_field_error(request, "wifi_password", "Wi-Fi password must be 64 characters or fewer.");
    return;
  }
  if (!valid_wpa_password(wifi_password)) {
    send_field_error(request, "wifi_password", "Use 8 to 63 printable ASCII characters or 64 hexadecimal digits.");
    return;
  }
  if (!mqtt_enabled) {
    memset(next.mqtt_broker, 0, sizeof(next.mqtt_broker));
    memset(next.mqtt_username, 0, sizeof(next.mqtt_username));
    memset(next.mqtt_password, 0, sizeof(next.mqtt_password));
  } else {
    if (broker.isEmpty()) {
      send_field_error(request, "mqtt_broker", "Enter an MQTT broker host name or IP address.");
      return;
    }
    if (!copy_value(next.mqtt_broker, sizeof(next.mqtt_broker), broker)) {
      send_field_error(request, "mqtt_broker", "MQTT broker host must be 64 characters or fewer.");
      return;
    }
    if (!copy_value(next.mqtt_username, sizeof(next.mqtt_username), username)) {
      send_field_error(request, "mqtt_username", "MQTT username must be 32 characters or fewer.");
      return;
    }
    if (!copy_value(next.mqtt_password, sizeof(next.mqtt_password), mqtt_password)) {
      send_field_error(request, "mqtt_password", "MQTT password must be 64 characters or fewer.");
      return;
    }
    if (!username.isEmpty() && mqtt_password.isEmpty()) {
      send_field_error(request, "mqtt_password", "Enter an MQTT password when a username is provided.");
      return;
    }

    char *end = nullptr;
    const long port = strtol(port_text.c_str(), &end, 10);
    if (port_text.isEmpty()) {
      send_field_error(request, "mqtt_port", "Enter an MQTT port (default is 1883).");
      return;
    }
    if (*end != '\0' || port < 1 || port > 65535) {
      send_field_error(request, "mqtt_port", "Enter a whole number from 1 to 65535.");
      return;
    }
    next.mqtt_port = static_cast<uint16_t>(port);
  }

  const String ip_mode = request_arg(request, "ip_mode");
  if (ip_mode == "static") {
    next.ip_mode = 1;
    if (!parse_ipv4(request_arg(request, "static_ip"), next.static_ip)) {
      send_field_error(request, "static_ip", "Enter a valid device IPv4 address.");
      return;
    }
    if (!parse_ipv4(request_arg(request, "gateway"), next.gateway)) {
      send_field_error(request, "gateway", "Enter a valid gateway IPv4 address.");
      return;
    }
    if (!parse_ipv4(request_arg(request, "subnet"), next.subnet)) {
      send_field_error(request, "subnet", "Enter a valid subnet mask.");
      return;
    }
    const String dns = request_arg(request, "dns");
    memset(next.dns, 0, sizeof(next.dns));
    if (!dns.isEmpty() && !parse_ipv4(dns, next.dns)) {
      send_field_error(request, "dns", "Enter a valid DNS IPv4 address, or leave this blank.");
      return;
    }
  }

  if (!this->settings_preference_.save(&next) || !global_preferences->sync()) {
    request->send(500, "text/plain", "Could not save settings to flash");
    return;
  }
  this->settings_ = next;
  this->set_mqtt_enabled(mqtt_enabled);
  request->send(200, "text/plain", "Settings saved; rebooting");
  this->schedule_reboot_();
}

void P1Setup::schedule_reboot_() { this->reboot_at_ = millis() + 5000; }

}  // namespace esphome::p1_setup
