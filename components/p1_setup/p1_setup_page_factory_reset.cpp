#include "p1_setup_pages.h"

namespace esphome::p1_setup::web_pages {

const char FACTORY_RESET_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Factory Reset</title><style>
:root{font-family:system-ui,sans-serif;color:#14231d;background:#f3f6f2}body{margin:0 auto;padding:22px;max-width:620px}a{color:#27734f}h1{font-size:25px}p{color:#52645b;line-height:1.5}button{padding:11px 15px;border:0;border-radius:6px;background:#a33b2f;color:white;font:inherit;font-weight:650}.status{margin-top:14px}
</style></head><body><a href="/">&larr; Home</a><h1>Factory Reset</h1><p>This erases saved Wi-Fi, MQTT, and network settings, then restarts the device in setup mode.</p><button id="reset">Factory Reset</button><p class="status" id="status" role="status"></p><script>document.getElementById('reset').addEventListener('click',async()=>{if(!confirm('Erase all saved settings and restart?'))return;const s=document.getElementById('status');s.textContent='Resetting…';try{const r=await fetch('/api/factory-reset',{method:'POST'});if(!r.ok)throw new Error(await r.text());s.textContent='Settings erased. Device is restarting.'}catch(e){s.textContent=e.message}});</script></body></html>)HTML";

}  // namespace esphome::p1_setup::web_pages
