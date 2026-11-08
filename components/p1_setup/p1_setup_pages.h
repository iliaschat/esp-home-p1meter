#pragma once

#include <Arduino.h>

namespace esphome::p1_setup::web_pages {

extern const char CONFIGURE_FIELD_ERRORS[] PROGMEM;
extern const char ATTRIBUTION_FOOTER[] PROGMEM;
extern const char INDEX_PAGE[] PROGMEM;
extern const char CONFIGURE_PAGE[] PROGMEM;
extern const char OTA_PAGE[] PROGMEM;
extern const char FACTORY_RESET_PAGE[] PROGMEM;
extern const char REBOOT_PAGE[] PROGMEM;
extern const char PASSWORD_PAGE[] PROGMEM;

String render(const char *page, const char *device_name);

}  // namespace esphome::p1_setup::web_pages
