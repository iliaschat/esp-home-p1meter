#include "p1_setup_internal.h"

namespace esphome::p1_setup {

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

void P1Setup::send_html_(AsyncWebServerRequest *request, const String &page) {
  AsyncWebServerResponse *response = begin_text_response(request, 200, "text/html; charset=utf-8", page);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
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

}  // namespace esphome::p1_setup
