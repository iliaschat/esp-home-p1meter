#include "p1_setup_internal.h"

#ifdef USE_ESP32
#include "esphome/components/md5/md5.h"
#endif

namespace esphome::p1_setup {

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

namespace {

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

}  // namespace

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

bool same_origin(AsyncWebServerRequest *request) {
  String origin;
  String host;
  return request_header(request, "Origin", origin) && request_header(request, "Host", host) &&
         origin == String("http://") + host;
}

}  // namespace esphome::p1_setup
