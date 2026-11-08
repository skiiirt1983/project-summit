# firmware/web — config portal

`src/api.cpp`: the JSON API (status, Wi-Fi, firmware, settings export/import/reset/backup, diagnostics),
transport-agnostic and host-tested. `esp/http_server.cpp`: esp_http_server adapter, streaming firmware
upload into the update engine. `page/index.html`: the single-page UI (vanilla JS), gzipped into flash.
The PC simulator serves the same page and API: `build/sim/summit_sim --web 8080`.
