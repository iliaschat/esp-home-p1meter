#include "p1_setup_internal.h"

namespace esphome::p1_setup {

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
  if (request->method() == HTTP_POST && request_url(request) == "/update")
    return true;
  return wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active() &&
         is_softap_client(request) && this->setup_->canHandle(request);
}

void APSetupHandler::handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST && !same_origin(request)) {
    request->send(403, "text/plain", "Invalid request origin");
    return;
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
  if (!same_origin(request))
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

}  // namespace esphome::p1_setup
