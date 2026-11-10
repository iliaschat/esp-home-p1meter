#include "p1_setup_pages.h"

namespace esphome::p1_setup::web_pages {

String render(const char *page, const char *device_name) {
	String escaped_name;
	for (const char *character = device_name; *character != '\0'; character++) {
		switch (*character) {
			case '&':
				escaped_name += F("&amp;");
				break;
			case '<':
				escaped_name += F("&lt;");
				break;
			case '>':
				escaped_name += F("&gt;");
				break;
			case '"':
				escaped_name += F("&quot;");
				break;
			case '\'':
				escaped_name += F("&#39;");
				break;
			default:
				escaped_name += *character;
		}
	}
	String rendered = FPSTR(page);
	rendered.replace(F("P1 Meter"), escaped_name);
	rendered.replace(F("P1 meter"), escaped_name);
	rendered.remove(rendered.length() - 14);
	rendered += FPSTR(ATTRIBUTION_FOOTER);
	rendered += F("</body></html>");
	return rendered;
}

}  // namespace esphome::p1_setup::web_pages
