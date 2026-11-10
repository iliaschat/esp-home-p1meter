#include "p1_setup_pages.h"

namespace esphome::p1_setup::web_pages {

const char REBOOT_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Reboot</title><style>
:root{font-family:system-ui,sans-serif;color:#14231d;background:#f3f6f2}body{margin:0 auto;padding:22px;max-width:620px}a{color:#27734f}h1{font-size:25px}p{color:#52645b;line-height:1.5}button{padding:11px 15px;border:0;border-radius:6px;background:#27734f;color:white;font:inherit;font-weight:650}.status{margin-top:14px}
</style></head><body><a href="/">&larr; Home</a><h1>Reboot device</h1><p>Restart the meter now.</p><button id="reboot">Reboot</button><p class="status" id="status" role="status"></p><script>document.getElementById('reboot').addEventListener('click',async()=>{if(!confirm('Reboot the P1 meter now?'))return;const s=document.getElementById('status');s.textContent='Rebooting…';try{const r=await fetch('/api/reboot',{method:'POST'});if(!r.ok)throw new Error(await r.text());s.textContent='Device is restarting.'}catch(e){s.textContent=e.message}});</script></body></html>)HTML";

}  // namespace esphome::p1_setup::web_pages
