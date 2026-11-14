#pragma once

#include <cstdlib>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "p1_setup.h"
#include "p1_setup_pages.h"

#ifdef USE_ESP32
#include <WiFi.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "esp_http_server.h"
#endif

namespace esphome::p1_setup {

inline constexpr uint32_t SETTINGS_MAGIC = 0x50315331;
inline constexpr uint32_t SETTINGS_PREFERENCE_KEY = 0x50315331;
inline constexpr uint32_t MQTT_ENABLED_PREFERENCE_KEY = 0x50314D45;
inline constexpr uint32_t SETUP_PASSWORD_MAGIC = 0x50315350;
inline constexpr uint32_t SETUP_PASSWORD_PREFERENCE_KEY = 0x50315350;
inline constexpr uint32_t HISTORY_MAGIC = 0x50314831;
inline constexpr uint32_t HISTORY_PREFERENCE_KEY = 0x50314831;
inline constexpr uint32_t SESSION_TIMEOUT_MS = 30UL * 60UL * 1000UL;
inline constexpr size_t SESSION_TOKEN_LENGTH = 32;

struct SetupPassword {
  uint32_t magic;
  char password[65];
};

// Request helpers shared by the HTTP, auth, and route code.
String request_url(AsyncWebServerRequest *request);
bool request_header(AsyncWebServerRequest *request, const char *name, String &value);
String request_arg(AsyncWebServerRequest *request, const char *name);
bool is_softap_client(AsyncWebServerRequest *request);
AsyncWebServerResponse *begin_text_response(AsyncWebServerRequest *request, int code, const char *content_type,
                                            const String &body);
bool digest_login_ok(AsyncWebServerRequest *request, const char *username, const char *password, const char *realm);
void send_digest_challenge(AsyncWebServerRequest *request, const char *realm);
bool same_origin(AsyncWebServerRequest *request);

// Validation and JSON helpers.
bool copy_value(char *destination, size_t capacity, const String &value);
bool valid_setup_password(const String &password);
bool valid_wpa_password(const String &password);
bool parse_ipv4(const String &value, uint8_t destination[4]);
void append_json_string(String &output, const char *value);
void send_field_error(AsyncWebServerRequest *request, const char *field, const char *message);
void append_json_ip(String &output, const uint8_t address[4]);

}  // namespace esphome::p1_setup
