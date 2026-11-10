#include "p1_setup_internal.h"

namespace esphome::p1_setup {

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

}  // namespace esphome::p1_setup
