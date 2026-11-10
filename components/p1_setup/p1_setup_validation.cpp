#include "p1_setup_internal.h"

namespace esphome::p1_setup {

bool copy_value(char *destination, size_t capacity, const String &value) {
  if (value.length() >= capacity)
    return false;
  memset(destination, 0, capacity);
  memcpy(destination, value.c_str(), value.length());
  return true;
}

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

}  // namespace esphome::p1_setup
