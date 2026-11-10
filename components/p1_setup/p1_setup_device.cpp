#include "p1_setup_internal.h"

namespace esphome::p1_setup {

void P1Setup::send_scan_(AsyncWebServerRequest *request) {
  const bool ap_active = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active();
  if (!this->scan_active_) {
    WiFi.mode(ap_active ? WIFI_AP_STA : WIFI_STA);
    const int started = WiFi.scanNetworks(true, true);
    if (started == WIFI_SCAN_RUNNING) {
      this->scan_active_ = true;
      request->send(202, "application/json", "{\"scanning\":true}");
      return;
    }
    if (started < 0) {
      request->send(503, "text/plain", "Wi-Fi scan failed; try again");
      return;
    }
  }

  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    request->send(202, "application/json", "{\"scanning\":true}");
    return;
  }
  if (count < 0) {
    this->scan_active_ = false;
    WiFi.scanDelete();
    request->send(503, "text/plain", "Wi-Fi scan failed; try again");
    return;
  }

  String json;
  json.reserve(static_cast<size_t>(count) * 64 + 2);
  json += '[';
  for (int i = 0; i < count; i++) {
    if (i != 0)
      json += ',';
    json += F("{\"ssid\":");
    String ssid = WiFi.SSID(i);
    append_json_string(json, ssid.c_str());
    json += F(",\"rssi\":");
    json += String(WiFi.RSSI(i));
    json += '}';
  }
  json += ']';
  this->scan_active_ = false;
  WiFi.scanDelete();
  request->send(200, "application/json", json.c_str());
}

}  // namespace esphome::p1_setup
