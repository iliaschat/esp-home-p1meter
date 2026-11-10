#include "p1_setup_pages.h"

namespace esphome::p1_setup::web_pages {

const char OTA_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>OTA Update</title><style>
:root{font-family:system-ui,sans-serif;color:#14231d;background:#f3f6f2}*{box-sizing:border-box}body{margin:0 auto;padding:22px;max-width:620px}a{color:#27734f}h1{font-size:25px;margin:16px 0}p{color:#52645b;line-height:1.5}.field{display:grid;gap:8px;margin:20px 0}input{padding:12px;border:1px solid #bccbc1;border-radius:6px;background:#fff;font:inherit}button{padding:11px 15px;border:0;border-radius:6px;background:#27734f;color:white;font:inherit;font-weight:650}.status{margin-top:14px;min-height:24px}
</style></head><body><a href="/">&larr; Home</a><h1>OTA Update</h1><p>Select the ESPHome OTA firmware image (usually named <code>firmware.ota.bin</code>). Keep power connected until the device restarts.</p><form id="upload"><div class="field"><label for="firmware">Firmware file</label><input id="firmware" name="firmware" type="file" accept=".bin" required></div><button>Upload firmware</button><div id="status" class="status" role="status"></div></form><script>document.getElementById('upload').addEventListener('submit',async e=>{e.preventDefault();const s=document.getElementById('status');s.textContent='Uploading... do not disconnect power.';try{const r=await fetch('/update',{method:'POST',body:new FormData(e.target)});const t=await r.text();if(!r.ok)throw new Error(t);s.textContent='Update accepted. Device is rebooting.'}catch(err){s.textContent=err.message}});</script></body></html>)HTML";

}  // namespace esphome::p1_setup::web_pages
