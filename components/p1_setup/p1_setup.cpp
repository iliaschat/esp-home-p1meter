#include "p1_setup.h"
#include "p1_setup_pages.h"

#include <cstring>
#include <cstdlib>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32
#include <WiFi.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "esp_http_server.h"
#include "esphome/components/md5/md5.h"
#endif

namespace esphome::p1_setup {
namespace {

static constexpr uint32_t SETTINGS_MAGIC = 0x50315331;
static constexpr uint32_t SETTINGS_PREFERENCE_KEY = 0x50315331;
static constexpr uint32_t MQTT_ENABLED_PREFERENCE_KEY = 0x50314D45;
static constexpr uint32_t SETUP_PASSWORD_MAGIC = 0x50315350;
static constexpr uint32_t SETUP_PASSWORD_PREFERENCE_KEY = 0x50315350;
static constexpr uint32_t SESSION_TIMEOUT_MS = 30UL * 60UL * 1000UL;
static constexpr size_t SESSION_TOKEN_LENGTH = 32;

// Request helpers. ESP8266 uses ESPAsyncWebServer; ESP32 uses ESPHome's web_server_idf.
String request_url(AsyncWebServerRequest *request) {
#ifdef USE_ESP32
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(buffer);
  return url.empty() ? String() : String(url.c_str(), static_cast<unsigned int>(url.size()));
#else
  return request->url();
#endif
}

bool request_header(AsyncWebServerRequest *request, const char *name, String &value) {
#ifdef USE_ESP32
  const auto header = request->get_header(name);
  if (!header.has_value())
    return false;
  value = String(header.value().c_str());
  return true;
#else
  const AsyncWebHeader *header = request->getHeader(name);
  if (header == nullptr)
    return false;
  value = header->value();
  return true;
#endif
}

String request_arg(AsyncWebServerRequest *request, const char *name) {
#ifdef USE_ESP32
  return String(request->arg(name).c_str());
#else
  return request->arg(name);
#endif
}

// True when the request arrived on the setup access point's address (not a station client).
bool is_softap_client(AsyncWebServerRequest *request) {
#ifdef USE_ESP32
  const int socket = httpd_req_to_sockfd(static_cast<httpd_req_t *>(*request));
  if (socket < 0)
    return false;
  sockaddr_in local{};
  socklen_t length = sizeof(local);
  if (getsockname(socket, reinterpret_cast<sockaddr *>(&local), &length) != 0)
    return false;
  return IPAddress(local.sin_addr.s_addr) == WiFi.softAPIP();
#else
  return request->client() != nullptr && request->client()->localIP() == WiFi.softAPIP();
#endif
}

AsyncWebServerResponse *begin_text_response(AsyncWebServerRequest *request, int code, const char *content_type,
                                            const String &body) {
#ifdef USE_ESP32
  return request->beginResponse(code, content_type, std::string(body.c_str(), body.length()));
#else
  return request->beginResponse(code, content_type, body);
#endif
}

#ifdef USE_ESP32
String md5_hex(const String &input) {
  md5::MD5Digest digest;
  digest.init();
  digest.add(reinterpret_cast<const uint8_t *>(input.c_str()), input.length());
  digest.calculate();
  char hex[33];
  digest.get_hex(hex);
  return String(hex);
}

// Reads one parameter from a Digest Authorization header, e.g. username="p1meter".
String digest_param(const String &header, const char *name) {
  const String key = String(name) + '=';
  int position = 0;
  while (true) {
    position = header.indexOf(key, position);
    if (position < 0)
      return String();
    if (position == 0 || header[position - 1] == ' ' || header[position - 1] == ',' || header[position - 1] == '\t')
      break;
    position += key.length();
  }
  const size_t start = static_cast<size_t>(position) + key.length();
  if (start < header.length() && header[start] == '"') {
    const int end = header.indexOf('"', start + 1);
    if (end < 0)
      return String();
    return header.substring(start + 1, end);
  }
  int end = header.indexOf(',', start);
  if (end < 0)
    end = header.length();
  String value = header.substring(start, end);
  value.trim();
  return value;
}

// Verifies an RFC 7616 MD5 Digest response (qop=auth or no qop) for the setup login.
bool digest_authenticated(AsyncWebServerRequest *request, const char *username, const char *password,
                          const char *realm) {
  String header;
  if (!request_header(request, "Authorization", header) || !header.startsWith("Digest "))
    return false;
  const String user = digest_param(header, "username");
  const String response = digest_param(header, "response");
  const String nonce = digest_param(header, "nonce");
  if (user != username || digest_param(header, "realm") != realm || nonce != realm)
    return false;
  const char *raw_uri = static_cast<httpd_req_t *>(*request)->uri;
  const String uri = digest_param(header, "uri");
  if (uri != raw_uri)
    return false;
  const String method = request->method() == HTTP_POST ? "POST" : "GET";
  const String qop = digest_param(header, "qop");
  const String ha1 = md5_hex(user + ':' + String(realm) + ':' + String(password));
  const String ha2 = md5_hex(method + ':' + uri);
  String expected;
  if (qop.isEmpty()) {
    expected = md5_hex(ha1 + ':' + nonce + ':' + ha2);
  } else if (qop == "auth") {
    expected = md5_hex(ha1 + ':' + nonce + ':' + digest_param(header, "nc") + ':' + digest_param(header, "cnonce") +
                       ':' + qop + ':' + ha2);
  } else {
    return false;
  }
  return expected.equalsIgnoreCase(response);
}
#endif

bool digest_login_ok(AsyncWebServerRequest *request, const char *username, const char *password, const char *realm) {
#ifdef USE_ESP32
  return digest_authenticated(request, username, password, realm);
#else
  return request->authenticate(username, password, realm);
#endif
}

void send_digest_challenge(AsyncWebServerRequest *request, const char *realm) {
#ifdef USE_ESP32
  String challenge = F("Digest realm=\"");
  challenge += realm;
  challenge += F("\", qop=\"auth\", nonce=\"");
  challenge += realm;
  challenge += '"';
  AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication required");
  response->addHeader("WWW-Authenticate", challenge.c_str());
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
#else
  request->requestAuthentication(realm, true);
#endif
}


bool copy_value(char *destination, size_t capacity, const String &value) {
  if (value.length() >= capacity)
    return false;
  memset(destination, 0, capacity);
  memcpy(destination, value.c_str(), value.length());
  return true;
}

struct SetupPassword {
  uint32_t magic;
  char password[65];
};

bool valid_setup_password(const String &password) {
  const size_t length = password.length();
  if (length < 8 || length > 63)
    return false;
  for (size_t index = 0; index < length; index++) {
    const unsigned char character = password[index];
    if (character < 0x20 || character > 0x7e)
      return false;
  }
  return true;
}

bool valid_wpa_password(const String &password) {
  const size_t length = password.length();
  if (length < 8 || length > 64)
    return false;
  for (size_t index = 0; index < length; index++) {
    const unsigned char character = password[index];
    if (length == 64) {
      if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F')))
        return false;
    } else if (character < 0x20 || character > 0x7e) {
      return false;
    }
  }
  return true;
}

bool parse_ipv4(const String &value, uint8_t destination[4]) {
  IPAddress parsed;
  if (!parsed.fromString(value))
    return false;
  for (size_t i = 0; i < 4; i++)
    destination[i] = parsed[i];
  return true;
}

void append_json_string(String &output, const char *value) {
  output += '"';
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p != 0; p++) {
    switch (*p) {
      case '"':
        output += F("\\\"");
        break;
      case '\\':
        output += F("\\\\");
        break;
      case '\n':
        output += F("\\n");
        break;
      case '\r':
        output += F("\\r");
        break;
      case '\t':
        output += F("\\t");
        break;
      default:
        if (*p >= 0x20)
          output += static_cast<char>(*p);
    }
  }
  output += '"';
}

void send_field_error(AsyncWebServerRequest *request, const char *field, const char *message) {
  String json;
  json.reserve(128);
  json += F("{\"field\":");
  append_json_string(json, field);
  json += F(",\"message\":");
  append_json_string(json, message);
  json += '}';
  request->send(400, "application/json", json.c_str());
}

void append_json_ip(String &output, const uint8_t address[4]) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
  append_json_string(output, buffer);
}

}  // namespace

bool CaptiveProbeHandler::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() != HTTP_GET || wifi::global_wifi_component == nullptr ||
      !wifi::global_wifi_component->is_ap_active())
    return false;
  const String url = request_url(request);
  if (url == "/generate_204" || url == "/gen_204" || url == "/hotspot-detect.html" ||
      url == "/library/test/success.html" || url == "/connecttest.txt" || url == "/ncsi.txt" ||
      url == "/success.txt" || url == "/redirect" || url == "/canonical.html")
    return true;
  // Any other request from a setup AP client addressed to a non-AP host is a probe.
  String host;
  return is_softap_client(request) && (!request_header(request, "Host", host) || host != "192.168.4.1");
}

void CaptiveProbeHandler::handleRequest(AsyncWebServerRequest *request) {
  request->redirect("http://192.168.4.1/");
}

bool APSetupHandler::canHandle(AsyncWebServerRequest *request) const {
  return wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active() &&
         is_softap_client(request) &&
         (this->setup_->canHandle(request) || (request->method() == HTTP_POST && request_url(request) == "/update"));
}

void APSetupHandler::handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    String origin;
    if (!request_header(request, "Origin", origin) || origin != "http://192.168.4.1") {
      request->send(403, "text/plain", "Invalid request origin");
      return;
    }
  }
  if (request_url(request) == "/update") {
    if (!this->setup_->session_authenticated_(request)) {
      request->send(401, "text/plain", "Setup session expired");
      return;
    }
    if (this->upload_request_ != request) {
      request->send(409, "text/plain", "Another firmware update is in progress");
    } else if (this->upload_success_) {
      this->upload_request_ = nullptr;
      request->send(200, "text/plain", "Update successful; rebooting");
      this->setup_->schedule_reboot_();
    } else {
      this->upload_request_ = nullptr;
      request->send(500, "text/plain", "Firmware update failed");
    }
    return;
  }
  this->setup_->handleRequest(request);
}

void APSetupHandler::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                  uint8_t *data, size_t len, bool final) {
  if (request_url(request) != "/update")
    return;
  if (!this->setup_->session_authenticated_(request))
    return;
  String origin;
  if (!request_header(request, "Origin", origin) || origin != "http://192.168.4.1")
    return;
  if (index == 0 && len > 0) {
#ifdef USE_ESP32
    // web_server_idf has no onDisconnect(); drop any upload left behind by an aborted connection.
    if (this->ota_backend_ != nullptr) {
      this->ota_backend_->abort();
      this->ota_backend_.reset();
    }
    this->upload_request_ = nullptr;
#endif
    if (this->upload_request_ != nullptr)
      return;
    this->upload_request_ = request;
    this->upload_success_ = false;
#ifndef USE_ESP32
    request->onDisconnect([this, request]() {
      if (this->upload_request_ == request) {
        if (this->ota_backend_ != nullptr) {
          this->ota_backend_->abort();
          this->ota_backend_.reset();
        }
        this->upload_request_ = nullptr;
      }
    });
#endif
    this->ota_backend_ = ota::make_ota_backend();
    if (this->ota_backend_ == nullptr || this->ota_backend_->begin(0) != ota::OTA_RESPONSE_OK) {
      if (this->ota_backend_ != nullptr) {
        this->ota_backend_->abort();
        this->ota_backend_.reset();
      }
      return;
    }
  }
  if (this->upload_request_ != request || this->ota_backend_ == nullptr)
    return;
  if (len > 0 && this->ota_backend_->write(data, len) != ota::OTA_RESPONSE_OK) {
    this->ota_backend_->abort();
    this->ota_backend_.reset();
    return;
  }
  if (final) {
    this->upload_success_ = this->ota_backend_->end() == ota::OTA_RESPONSE_OK;
    if (!this->upload_success_)
      this->ota_backend_->abort();
    this->ota_backend_.reset();
  }
}

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

bool P1Setup::rotate_login_realm_() {
  uint8_t random_data[8];
  if (!random_bytes(random_data, sizeof(random_data)))
    return false;

  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  memcpy(this->login_realm_, "P1 Setup ", 9);
  for (size_t index = 0; index < sizeof(random_data); index++) {
    this->login_realm_[9 + index * 2] = HEX_DIGITS[random_data[index] >> 4];
    this->login_realm_[10 + index * 2] = HEX_DIGITS[random_data[index] & 0x0F];
  }
  this->login_realm_[25] = '\0';
  return true;
}

bool P1Setup::create_session_(char token[33]) {
  WebSession *available = nullptr;
  const uint32_t now = millis();
  for (WebSession &session : this->sessions_) {
    if (session.active && static_cast<uint32_t>(now - session.last_activity) < SESSION_TIMEOUT_MS)
      continue;
    session.active = false;
    session.token[0] = '\0';
    available = &session;
    break;
  }
  if (available == nullptr)
    return false;

  uint8_t random_data[16];
  if (!random_bytes(random_data, sizeof(random_data)))
    return false;

  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  for (size_t index = 0; index < sizeof(random_data); index++) {
    available->token[index * 2] = HEX_DIGITS[random_data[index] >> 4];
    available->token[index * 2 + 1] = HEX_DIGITS[random_data[index] & 0x0F];
  }
  available->token[SESSION_TOKEN_LENGTH] = '\0';
  available->last_activity = now;
  available->active = true;
  memcpy(token, available->token, sizeof(available->token));
  return true;
}

bool P1Setup::extract_session_token_(AsyncWebServerRequest *request, char token[33]) const {
  String cookies;
  if (!request_header(request, "Cookie", cookies))
    return false;

  size_t start = 0;
  while (start < cookies.length()) {
    const int separator = cookies.indexOf(';', start);
    const size_t end = separator < 0 ? cookies.length() : static_cast<size_t>(separator);
    String pair = cookies.substring(start, end);
    pair.trim();
    const int equals = pair.indexOf('=');
    if (equals > 0) {
      String name = pair.substring(0, equals);
      name.trim();
      if (name == "p1_session") {
        String value = pair.substring(equals + 1);
        value.trim();
        if (value.length() != SESSION_TOKEN_LENGTH)
          return false;
        for (size_t index = 0; index < SESSION_TOKEN_LENGTH; index++) {
          const char character = value[index];
          if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
          token[index] = character;
        }
        token[SESSION_TOKEN_LENGTH] = '\0';
        return true;
      }
    }
    if (separator < 0)
      break;
    start = static_cast<size_t>(separator) + 1;
  }
  return false;
}

bool P1Setup::session_authenticated_(AsyncWebServerRequest *request) {
  char token[33];
  if (!this->extract_session_token_(request, token))
    return false;

  const uint32_t now = millis();
  for (WebSession &session : this->sessions_) {
    if (!session.active)
      continue;
    if (static_cast<uint32_t>(now - session.last_activity) >= SESSION_TIMEOUT_MS) {
      session.active = false;
      session.token[0] = '\0';
      continue;
    }
    uint8_t difference = 0;
    for (size_t index = 0; index < SESSION_TOKEN_LENGTH; index++)
      difference |= static_cast<uint8_t>(session.token[index] ^ token[index]);
    if (difference == 0) {
      session.last_activity = now;
      return true;
    }
  }
  return false;
}

void P1Setup::authenticate_page_request_(AsyncWebServerRequest *request) {
  if (!this->login_realm_ready_ || this->login_username_.empty() || this->setup_password_.empty()) {
    request->send(503, "text/plain", "Setup sign-in is unavailable");
    return;
  }

  String authorization;
  if (!request_header(request, "Authorization", authorization) || !authorization.startsWith("Digest ") ||
      !digest_login_ok(request, this->login_username_.c_str(), this->setup_password_.c_str(), this->login_realm_)) {
    send_digest_challenge(request, this->login_realm_);
    return;
  }

  char token[33];
  if (!this->create_session_(token)) {
    request->send(503, "text/plain", "No setup session is available");
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(303, "text/plain", "");
  String cookie = F("p1_session=");
  cookie += token;
  cookie += F("; Path=/; HttpOnly; SameSite=Strict");
  const String url = request_url(request);
  response->addHeader("Set-Cookie", cookie.c_str());
  response->addHeader("Location", url.c_str());
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void P1Setup::handle_logout_(AsyncWebServerRequest *request) {
  String origin;
  String host;
  if (!request_header(request, "Origin", origin) || !request_header(request, "Host", host) ||
      origin != String("http://") + host) {
    request->send(403, "text/plain", "Invalid request origin");
    return;
  }

  char token[33];
  if (this->extract_session_token_(request, token)) {
    for (WebSession &session : this->sessions_) {
      uint8_t difference = 0;
      for (size_t index = 0; index < SESSION_TOKEN_LENGTH; index++)
        difference |= static_cast<uint8_t>(session.token[index] ^ token[index]);
      if (session.active && difference == 0) {
        session.active = false;
        session.token[0] = '\0';
        break;
      }
    }
  }

  this->login_realm_ready_ = this->rotate_login_realm_();
  if (!this->login_realm_ready_)
    ESP_LOGE("p1_setup", "Could not rotate the setup login realm after sign-out");

  AsyncWebServerResponse *response = begin_text_response(request, 200, "text/plain", String("Signed out"));
  response->addHeader("Set-Cookie", "p1_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void P1Setup::handle_unset_password_request_(AsyncWebServerRequest *request) {
  const String url = request_url(request);
  if (request->method() == HTTP_GET && (url == "/" || url == "/setup-password")) {
    String page = web_pages::render(web_pages::PASSWORD_PAGE, this->device_name_.c_str());
    this->send_html_(request, page);
  } else if (request->method() == HTTP_POST && url == "/api/setup-password") {
    this->save_setup_password_(request);
  } else if (request->method() == HTTP_GET && !url.startsWith("/api/")) {
    request->redirect("/");
  } else {
    request->send(403, "text/plain", "Set the setup password first");
  }
}

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

void P1Setup::send_html_(AsyncWebServerRequest *request, const String &page) {
  AsyncWebServerResponse *response = begin_text_response(request, 200, "text/html; charset=utf-8", page);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
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

bool P1Setup::canHandle(AsyncWebServerRequest *request) const {
  const String url = request_url(request);
  if (request->method() == HTTP_GET) {
      return url == "/" || url == "/configure" || url == "/ota" || url == "/factory-reset" || url == "/reboot" ||
        url == "/setup-password" || url == "/api/settings" || url == "/api/scan";
  }
  return request->method() == HTTP_POST &&
      (url == "/api/settings" || url == "/api/factory-reset" || url == "/api/reboot" || url == "/api/logout" ||
       url == "/api/setup-password");
}

void P1Setup::handleRequest(AsyncWebServerRequest *request) {
  const String url = request_url(request);
  if (this->setup_password_.empty()) {
    this->handle_unset_password_request_(request);
    return;
  }
  if (!this->session_authenticated_(request)) {
    if (request->method() == HTTP_GET && !url.startsWith("/api/")) {
      this->authenticate_page_request_(request);
    } else {
      request->send(401, "text/plain", "Setup session expired");
    }
    return;
  }
  if (request->method() == HTTP_POST && url == "/api/logout") {
    this->handle_logout_(request);
    return;
  }

  if (request->method() == HTTP_GET) {
    if (url == "/") {
      String page = web_pages::render(web_pages::INDEX_PAGE, this->device_name_.c_str());
      this->send_html_(request, page);
    } else if (url == "/configure") {
      this->send_configure_page_(request);
    } else if (url == "/ota") {
      String page = web_pages::render(web_pages::OTA_PAGE, this->device_name_.c_str());
      this->send_html_(request, page);
    } else if (url == "/factory-reset") {
      String page = web_pages::render(web_pages::FACTORY_RESET_PAGE, this->device_name_.c_str());
      this->send_html_(request, page);
    } else if (url == "/reboot") {
      String page = web_pages::render(web_pages::REBOOT_PAGE, this->device_name_.c_str());
      this->send_html_(request, page);
    } else if (url == "/setup-password") {
      String page = web_pages::render(web_pages::PASSWORD_PAGE, this->device_name_.c_str());
      this->send_html_(request, page);
    } else if (url == "/api/settings") {
      this->send_settings_(request);
    } else if (url == "/api/scan") {
      this->send_scan_(request);
    }
    return;
  }

  if (url == "/api/setup-password") {
    this->save_setup_password_(request);
  } else if (url == "/api/settings") {
    this->save_settings_(request);
  } else if (url == "/api/reboot") {
    request->send(200, "text/plain", "Rebooting");
    this->schedule_reboot_();
  } else if (url == "/api/factory-reset") {
    if (!global_preferences->reset()) {
      request->send(500, "text/plain", "Could not reset saved settings");
      return;
    }
    request->send(200, "text/plain", "Factory reset complete; rebooting");
    this->schedule_reboot_();
  }
}

void P1Setup::send_configure_page_(AsyncWebServerRequest *request) {
  constexpr size_t closing_length = 14;
  const size_t page_length = strlen_P(web_pages::CONFIGURE_PAGE) - closing_length;
  const size_t errors_length = strlen_P(web_pages::CONFIGURE_FIELD_ERRORS);
  const size_t attribution_length = strlen_P(web_pages::ATTRIBUTION_FOOTER);
#ifdef USE_ESP32
  // web_server_idf has no filler-callback response; stream the same pieces in order.
  AsyncResponseStream *response = request->beginResponseStream("text/html; charset=utf-8");
  response->print(std::string(web_pages::CONFIGURE_PAGE, page_length));
  response->print(std::string(web_pages::CONFIGURE_FIELD_ERRORS, errors_length));
  response->print(std::string(web_pages::ATTRIBUTION_FOOTER, attribution_length));
  response->print(std::string(web_pages::CONFIGURE_PAGE + page_length, closing_length));
#else
  const size_t total_length = page_length + errors_length + attribution_length + closing_length;
  AsyncWebServerResponse *response = request->beginResponse(
      "text/html; charset=utf-8", total_length,
      [page_length, errors_length, attribution_length,
       total_length](uint8_t *buffer, size_t max_length, size_t index) -> size_t {
        size_t written = 0;
        while (written < max_length && index < total_length) {
          size_t available;
          const char *source;
          if (index < page_length) {
            available = page_length - index;
            source = web_pages::CONFIGURE_PAGE + index;
          } else if (index < page_length + errors_length) {
            const size_t errors_index = index - page_length;
            available = errors_length - errors_index;
            source = web_pages::CONFIGURE_FIELD_ERRORS + errors_index;
          } else if (index < page_length + errors_length + attribution_length) {
            const size_t attribution_index = index - page_length - errors_length;
            available = attribution_length - attribution_index;
            source = web_pages::ATTRIBUTION_FOOTER + attribution_index;
          } else {
            const size_t closing_index = index - page_length - errors_length - attribution_length;
            available = closing_length - closing_index;
            source = web_pages::CONFIGURE_PAGE + page_length + closing_index;
          }
          size_t count = max_length - written;
          if (count > available)
            count = available;
          memcpy_P(buffer + written, source, count);
          written += count;
          index += count;
        }
        return written;
      });
#endif
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
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