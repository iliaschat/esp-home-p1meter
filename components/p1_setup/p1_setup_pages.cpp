#include "p1_setup_pages.h"
#include "esphome/core/defines.h"

#ifndef ESPHOME_PROJECT_VERSION
#define ESPHOME_PROJECT_VERSION "unknown"
#endif

namespace esphome::p1_setup::web_pages {

const char ATTRIBUTION_FOOTER[] PROGMEM = R"HTML(<footer style="display:flex;flex-wrap:wrap;gap:4px 14px;margin:32px 0 0;padding:14px 0 0;border-top:1px solid #d5dfd8;color:#62736a;font-size:12px;line-height:1.6"><span>Firmware version )HTML" ESPHOME_PROJECT_VERSION R"HTML(</span><span>Developed by <a href="https://github.com/iliaschat" target="_blank" rel="noopener noreferrer" style="color:#27734f;text-decoration:underline;text-underline-offset:2px">Ilias Chatzatoglou</a></span><span>Licensed under the <a href="https://polyformproject.org/licenses/noncommercial/1.0.0" target="_blank" rel="noopener noreferrer" style="color:#27734f;text-decoration:underline;text-underline-offset:2px">PolyForm Noncommercial License 1.0.0</a></span></footer>)HTML";

}  // namespace esphome::p1_setup::web_pages
