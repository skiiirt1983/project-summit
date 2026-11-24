# Project Summit — Phase 1 Design & Architecture

Nov 7, 2026 · Paul · Source of truth: https://claude.ai/artifact/8XW2p213uiCBZ6mtZwcVPq (this file is an export; diagrams are written out as text)

## Summary

SLOPEFORM Home is a battery-portable, auto-rotating 5" smart dashboard built on the Waveshare ESP32-P4-WIFI6-Touch-LCD-5, running native C++ firmware (ESP-IDF + LVGL) as one self-contained device — no companion server. Project Summit is the private alpha; SLOPEFORM Home is the public 1.0 on GitHub + MakerWorld.

**Decisions at a glance**

| Area | Decision | Why |
| --- | --- | --- |
| Firmware | C++ on ESP-IDF 5.5 + LVGL 9 | Same stack as PrintSphere (direct port), P4 2D accelerator, smooth 720p UI |
| Display mode | Landscape 1280×720 default, portrait 720×1280 supported | Panel is native portrait; rotation driven by LIS2DW12 hardware 4D detection |
| Power | 1S 3.7 V 8000 mAh LiPo on the board's own MX1.25 charge header | No extra charger board to buy |
| Accelerometer | I2C on the shared board bus + INT1 to one free GPIO | Orientation, double-tap and pick-up wake all run in the sensor, CPU sleeps |
| Companion | None — the device calls cloud services directly | One thing to build, flash and support; nothing extra for users to host |
| Config | Web portal served by the device + QR code on screen | No phone app to build or maintain |
| Apps | Compile-time panels in v1 → declarative widget packs in v1.x → sandboxed apps later | Scales without risking stability early |
| Updates | On-device Updates screen + web UI + USB recovery, from v0.1 | Modelled on LyricFlash's proven OTA design |
| Licence | GPL-3.0 | Forks that ship must publish source |
| Enclosure | Snap-fit printed shell + separate magnetic "slope" stand | Minimal hardware: magnets only, no screws required |

**Critical findings from the datasheets and board docs**

- The board already has a 3.7 V lithium charge/discharge header (MX1.25 2-pin), a power button (long-press 2 s = off), an RTC battery header, a speaker header (8 Ω 2 W speaker included) and onboard mics. ([Waveshare](https://www.waveshare.com/product/arduino/displays/lcd-mipi/esp32-p4-wifi6-touch-lcd-5.htm))
- Wi-Fi 6/BLE 5 comes from an ESP32-C6 co-processor over SDIO, not the P4 itself — the firmware must use the hosted Wi-Fi stack.
- **I2C address clash risk:** the ES8311 audio codec and the LIS2DW12 both live at 0x18/0x19. Scan the bus before wiring SA0.
- **Check battery connector polarity** before plugging in — JST/MX1.25 polarity varies by battery vendor.
- **PrintSphere's licence is listed as "other"**, and one fork calls itself a non-commercial derivative. Confirm it is GPL-compatible before porting.

## Core loop

The device turns five kinds of input into events, updates one shared state, and lets the visible panel react — so every physical action (rotate, chime, wake) is a consequence of state, never wired input-to-output.

```
INPUT                      PROCESS (firmware)              OUTPUT / ACTION
Touch (GT911)        ─┐    Services (Wi-Fi, time, audio)   ┌─ Animate the screen
Accelerometer events ─┤          ↓                         ├─ Rotate the layout
Mic audio            ─┼─▶  Event bus + state (one truth) ─▶├─ Chime, alarm, music
Network data         ─┤          ↓                         └─ Dim, wake, sleep
Power, USB, buttons  ─┘    Shell + panels (active view)
```

Five loops run side by side, each on its own FreeRTOS task so a slow network call never stutters the UI:

| Loop | Rate | Input → action |
| --- | --- | --- |
| UI render | 60 fps target, idle when nothing moves | Touch / gestures → page swipe, sheet, button |
| Orientation | Interrupt-driven (no polling) | LIS2DW12 4D change → debounce 400 ms → animated re-layout |
| Data refresh | Per panel: clock 1 s, printer live, weather 15 min, news 30 min, calendar 5 min | Network → state store → panels that subscribe redraw |
| Audio | Continuous while LyricFlash is active | Mic → FFT → visuals; clip → song ID → lyrics + song info |
| Power | 1 s battery poll + accelerometer wake | Idle timer, face-down, low battery → dim, ambient clock, sleep |

**Example end-to-end:** you pick the device up and turn it sideways → LIS2DW12 fires wake + 4D interrupts → power service raises backlight → orientation service publishes `orientation=landscape` → shell re-flows the panel grid with a 250 ms morph.

## Hardware components

Three bought parts plus printed parts: the board does compute, display, audio, Wi-Fi and battery charging, so the only wiring added is the battery plug and five wires to the accelerometer.

### Bill of materials

| # | Part | Qty | Role | Source |
| --- | --- | --- | --- | --- |
| 1 | Waveshare ESP32-P4-WIFI6-Touch-LCD-5 (includes 50 mm 8 Ω 2 W speaker) | 1 | Compute, 720×1280 IPS touch, audio, Wi-Fi 6/BLE 5, LiPo charging | Ordered |
| 2 | 1S 3.7 V 8000 mAh LiPo with protection circuit, MX1.25 2-pin lead | 1 | Portable power (≈29.6 Wh) | Ordered |
| 3 | LIS2DW12 accelerometer breakout | 1 | Rotation, tap, pick-up wake, face-down | Ordered |
| 4 | microSD card, 32 GB | 1 | Fonts, visual packs, alarm sounds, widget packs, caches (album art, lyrics, news images, last weather/calendar fetch) | Any |
| 5 | 5× female–female jumper (2.54 mm) or 30 AWG silicone wire | 5 | Accelerometer link to the 40-pin header | Any |
| 6 | Neodymium magnets (size set by stand design) | 4–8 | Magnetic stand, no screws | Bambu/MakerWorld hardware — verify listing |
| 7 | Printed parts: shell, bezel, stand, button plungers, TPU feet | — | Enclosure | PETG body, TPU feet |
| 8 | OV5647 camera (add-on, not v1) | 0–1 | Future presence detection | Optional |

### Board resources

| Resource | Detail | Used for |
| --- | --- | --- |
| ESP32-P4NRW32 | RISC-V dual-core, 32 MB PSRAM in package, 32 MB NOR flash | Firmware, framebuffers |
| Display | 5" IPS 720×1280, MIPI-DSI 2-lane, HX8394 driver, GT911 5-point touch | UI |
| ESP32-C6-MINI-1 | Wi-Fi 6 + BLE 5 co-processor over SDIO (P4 GPIO14–19) | All networking |
| ES8311 codec + NS4150B amp | I2S on GPIO9–13, amp enable GPIO53 | Alarm, chimes, music |
| Mics + echo cancellation | Onboard mic array (ES7210 per Waveshare) | LyricFlash listening |
| microSD | SDIO, power-switched | Asset storage |
| Shared I2C | SDA GPIO7, SCL GPIO8, 2.2 kΩ pull-ups on board | Touch, codec, accelerometer |
| LiPo header | MX1.25 2-pin, charge + discharge | Battery |
| Buttons | Power (long-press 2 s off), BOOT, RESET | Need plungers in enclosure |
| USB-C ×2 | UART port (power, flashing) + USB 2.0 HS OTG | Charging, dev, future USB accessories |
| 40-pin header | 2.54 mm, Pi-HAT style | Accelerometer, future add-ons |

### Accelerometer wiring (proposed)

| LIS2DW12 pin | Board | Note |
| --- | --- | --- |
| VDD + VDD_IO | 3V3 on 40-pin header | Datasheet range 1.62–3.6 V |
| GND | GND | |
| SCL | GPIO8 (I2C SCL) | Shared bus, 400 kHz |
| SDA | GPIO7 (I2C SDA) | Shared bus |
| INT1 | GPIO4 (proposed, D-012) | 4D orientation, double-tap, wake — routed to one pin. Must be an LP GPIO (0–15) to wake the chip from deep sleep |
| INT2 | GPIO5 (optional) | Spare for sleep-state change |
| CS | Tie to VDD_IO | Forces I2C mode |
| SA0 | Set after bus scan | Picks 0x18 or 0x19 |

GPIO4/5 come from the header's net list in the schematic. Before wiring, confirm them against the ESP32-P4 datasheet's LP GPIO list and the board silkscreen; if they are not usable, pick another free GPIO in 0–15 (GPIO21/22, the earlier proposal, cannot wake the chip). The code constants are in `firmware/main/board_pins.h`.

### I2C address map

| Device | Address | Status |
| --- | --- | --- |
| GT911 touch | 0x5D or 0x14 | Fixed by board |
| ES8311 codec | 0x18 (CE low) or 0x19 (CE high) | Fixed by board, unknown which |
| ES7210 echo canceller | 0x40–0x43 | Fixed by board |
| LIS2DW12 | 0x18 (SA0 low) or 0x19 (SA0 high) | **Must be the one the codec is not using** |

First firmware task on hardware: an I2C scan with the accelerometer unplugged. If the codec answers at 0x18, tie SA0 high (0x19); if at 0x19, tie SA0 to GND. WHO_AM_I must read 0x44.

### Power and wiring constraints

- **Battery:** ≈29.6 Wh. Planning estimate (to be measured): 1.5–2.5 W with screen on gives roughly 10–17 h; ambient clock mode should stretch that to days.
- **Charge time** depends on the board's undocumented charger current: ~9 h at 1 A, ~17 h at 0.5 A. Measure in week 1.
- **Battery protection:** protected pack only; check polarity with a multimeter before connecting.
- **Keep the LiPo ≥10 mm from the C6 module's PCB antenna**, no metal near it.
- **I2C wires ≤15 cm.** Breakout pull-ups in parallel with the board's 2.2 kΩ are fine.
- **Mount the accelerometer rigidly** to the same part as the display, axes square to the screen; record its axis mapping in config.
- **Heat:** the LiPo must not sit directly on the P4 / display driver.

## Physical dimensions and enclosure rules

Only the active area is fixed today (≈110.7 × 62.3 mm, from 5" at 16:9). Everything else is a named parameter, measured when parts arrive and frozen in `enclosure/dimensions` (single source for CAD and docs).

### Parameters to lock in week 1

| Parameter | Value | How to get it |
| --- | --- | --- |
| `DISP_ACTIVE_W × H` | ≈110.7 × 62.3 mm | Calculated; confirm on panel |
| `BOARD_W × H × T` (incl. glass) | TBD | Waveshare outline drawing + calipers |
| `GLASS_BORDER` each side | TBD | Calipers, 4 sides |
| Mounting holes: count, Ø, centres | TBD | Outline drawing |
| Tallest rear component height | TBD | Header, USB-C, speaker connector |
| USB-C port centres + depth from glass | TBD | Both ports need cable access |
| Power / BOOT / RESET button positions | TBD | Plunger locations |
| Mic port position(s) | TBD | Clear acoustic path |
| C6 antenna keep-out zone | TBD | Mark on board photo |
| `BATT_L × W × T` | TBD (typically ~10–12 mm thick) | Calipers |
| Battery lead length + exit side | TBD | Decides battery orientation |
| Accelerometer breakout `L × W`, holes | TBD | Calipers |
| Speaker Ø 50 mm + depth | Depth TBD | Calipers |

### Print tolerances (Bambu P2S, 0.4 mm nozzle, PETG)

| Feature | Clearance / size | Note |
| --- | --- | --- |
| PCB / glass pocket | +0.30 mm per side | |
| Snap-fit lid seam | 0.15–0.20 mm per side | Test coupon first |
| Sliding fit (stand, battery tray) | 0.25 mm per side | |
| Round holes (vertical) | +0.20 mm on Ø | Horizontal: teardrop or +0.3 mm |
| Magnet pockets | Ø +0.10 mm press, or +0.20 mm + glue | Print pocket face-down |
| Walls | 2.0–2.4 mm (5 perimeters) | |
| Bezel lip over glass | 1.5–2.0 mm, 0.2 mm off active area | |
| Battery swell allowance | +1.0 mm thickness | TPU/foam pad, no hard clamp |
| Air gap battery ↔ board | ≥2 mm or printed divider | |
| Button plunger travel | 0.6–0.8 mm over-travel, 0.3 mm guide clearance | Flexure plunger |
| Mic and speaker ports | Grille ≥30% open, mic hole Ø1.5–2 mm aligned | |

### Enclosure concept: tablet body + magnetic slope stand

- **Body:** bezel + rear shell, printed snap tabs, board held by a clip-in retaining frame.
- **Battery:** printed tray, divider/air gap, TPU strap — tool-free swap.
- **Accelerometer:** cradle on the retaining frame, axes square to the glass.
- **Speaker:** sealed printed back-chamber.
- **Stand ("the slope"):** magnetic wedge at ~15–20°, works in landscape and portrait.
- **Recovery access:** UART USB-C port and BOOT/RESET plungers must stay reachable.
- **Depth estimate:** ~25–30 mm.

## Software stack

All code runs on the device: C++ on ESP-IDF 5.5 with LVGL 9, plus a config web page the device serves itself. **One device, no companion server** — it talks to cloud/LAN services directly over HTTPS/MQTT.

```
SLOPEFORM Home device (C++)                 Cloud and LAN services
┌──────────────────────────────┐            ┌──────────────────────────────┐
│ Panels: Home, Weather, News,  │            │ LyricFlash service           │
│   LyricFlash, Printer         │            │   song ID + synced lyrics    │
│ Shell + UI kit                │  direct    │ Open-Meteo + RSS feeds       │
│ App framework  ◀── the seam   │ ◀────────▶ │ Google Calendar (ICS link)   │
│ Services                      │            │ Bambu printer (MQTT LAN/cloud)│
│ Board support + microSD cache │            └──────────────────────────────┘
└──────────────────────────────┘
```

### What runs where

| Where | Language / tools | Responsibilities |
| --- | --- | --- |
| Device firmware | C++17, ESP-IDF 5.5.x, LVGL 9.x, FreeRTOS, esp_hosted (C6 Wi-Fi), esp-mqtt, esp_http_client, cJSON, esp-dsp (FFT), esp_codec_dev, NVS, FAT on microSD, esp_https_ota | UI, rotation, clock/alarm, weather, ICS calendar, RSS, printer, audio visuals, power, updates |
| Device web config | Small Preact or vanilla JS page, gzipped into flash | Wi-Fi setup (AP + QR), panel order, feeds, calendars, printer creds, theme, firmware, settings backup |
| CI | GitHub Actions | Host tests, firmware build, OTA + full image packaging, web flasher page |

### Firmware layout

```
firmware/
  main/            app_main, boot sequence
  bsp/             board, display, touch, audio, sd, battery
  services/        net, time, http, mqtt, audio_dsp, imu, power, ota, storage
  framework/       event_bus, state_store, settings, panel_api, registry
  shell/           pager, status_bar, sheets, toasts, ambient, theme, rotation
  panels/          home_clock, calendar, weather, news, lyricflash, printer
  web/             config portal
```

### Panel contract (the scaling seam)

- **Manifest:** id, name, icon, version, sizes (full / half / tile), orientations, data subscriptions, settings schema (auto-rendered in web config).
- **Lifecycle:** `create → show → tick → hide → destroy`; hidden panels hold no LVGL objects.
- **Data:** panels read the state store and subscribe to topics (`weather/now`, `printer/status`, `audio/spectrum`); services do all network I/O.
- **Layout:** size tokens, never pixels — one layout for portrait and landscape.

### Web config portal

A single vanilla-JS page served gzipped from flash (`firmware/web/page/index.html`) over a JSON API (`firmware/web/src/api.cpp`, pure and host-tested; the PC simulator serves the same page and API with `summit_sim --web PORT`). Tabs: Status, Wi-Fi, Firmware, Settings, Diagnostics.

| Endpoint | Purpose |
| --- | --- |
| `GET /api/status` | firmware, hardware ID, Wi-Fi, battery, settings notice, update state |
| `GET /api/log[?lines=N][&download=1]`, `POST /api/log/clear` | the console log as plain text (colour codes removed), kept in a 48 KB ring in memory; the Log tab has Copy, Download, Clear and a 2 s refresh |
| `GET/POST /api/wifi` | status + scan results; save SSID + password (password is write-only) |
| `GET /api/firmware` | version, channel, update stage/progress/error, available build + notes, catalog list |
| `POST /api/firmware/check`, `/install`, `/reboot`, `/previous` | update commands (previous needs `{"confirm":true}`) |
| `POST /api/firmware/upload` | raw OTA `.bin`, streamed into the update engine; needs header `X-Summit-Upload: 1` |
| `GET/POST /api/settings` | schema + values (the form is rendered from it); set values by key |
| `GET /api/settings/export[?secrets=1]` | settings file (`format: summit-settings`, schema version, values, Wi-Fi SSID); secrets only with `secrets=1` |
| `POST /api/settings/import/validate`, `POST /api/settings/import` | check a whole file without changing anything; replace all settings |
| `POST /api/settings/undo` | undo the last import (restores the backup) |
| `POST /api/settings/reset` | defaults (needs `{"confirm":true}`); `GET /api/settings/backup` downloads the pre-reset/import backup |
| `GET /api/diagnostics` | warnings (e.g. missing add-ons), settings/update state, event bus stats, device facts |

- Every firmware path goes through the one update engine (hard rule 5); uploads get the same header, size and image checks as catalog installs.
- Wi-Fi credentials live in their own NVS namespace, so a settings export never contains them (hard rule 6).
- Settings import replaces all settings. The whole file is validated first: a file from a newer schema or with any invalid value is refused with a clear message and nothing changes. The page then asks "Replace all settings? Your current settings are backed up." before applying. Older files are migrated, missing keys get defaults, unknown keys are kept. The backup stays available: "Undo import" restores it and it can be downloaded.
- Secrets (Wi-Fi password, printer access code, tokens: settings marked `secret`) are left out of exports by default; a tick-box includes them. An import keeps the current secrets when the file has none. The API never returns a secret's value, only whether it is set.
- Settings schema entries can carry an optional `label` and `help` text (shown by the web UI instead of the raw key). Raw keys are fine for the alpha; labels and help are filled in before the first beta. These are code-only and don't change stored settings.
- Security: no login. The local network is trusted. Cross-site protection stays: POST bodies must be `application/json` and uploads need the `X-Summit-Upload` header, so other web pages can't trigger them without a CORS preflight the device never approves.
- Wi-Fi setup (separate hardware step): the setup access point uses WPA2 with a password shown in the on-screen QR code, never an open network.

### State topics

The state store and event bus are the public API between services, panels and the shell. Names follow `domain/item` (lowercase, `[a-z0-9_]`). Keep this table updated: `tests/test_state_topics_contract.py` fails if code uses a topic missing from it. "event" topics are published on the bus only and hold no stored state.

| Topic | Type | Published by | Read by |
| --- | --- | --- | --- |
| `time/now_text` | string, e.g. "09:41" | time service | glance strip (time chip), Home |
| `time/date_text` | string | time service | Home |
| `calendar/next_text` | string: next event + countdown; empty = none | calendar service | glance strip (next-event chip), Home |
| `weather/now_text` | string: temp + condition; empty = none | weather service | glance strip (weather chip), Home, Weather |
| `printer/progress_text` | string; empty when not printing | printer service | glance strip (printer chip), Printer |
| `printer/progress_pct` | int 0–100 | printer service | Printer |
| `net/wifi_connected` | bool | net service | glance strip (Wi-Fi chip), web UI |
| `net/ssid` | string: connected network | net service | web UI |
| `net/ip` | string: IPv4 address | net service | web UI |
| `net/last_error` | string: why the last connection attempt failed, in words ("Wrong password? (code 15)"); empty when connected | net service | quick-settings sheet, web UI Wi-Fi tab |
| `net/ap_active` | bool: the setup access point (Summit-Setup) is on | net service | quick-settings sheet (shows how to join it) |
| `net/scan` | string: JSON array of {ssid, rssi, secure} | net service | web UI (Wi-Fi page) |
| `net/connect` | event: SSID just saved (the password stays in the Wi-Fi store) | web UI | net service |
| `net/wifi_chip_command` | event: string, `probe` = try the Wi-Fi chip update handshake (flashes nothing) | web UI (Diagnostics tab) | net service |
| `diag/flash_test` | event: string `run` = run the flash (above 16 MB) and PSRAM self-test in its own task; touches only the unused `assets` partition and 5 MB of PSRAM. Result in `diag/metrics` (`flash_selftest`, `flash_last_verify`) | web UI Diagnostics (`POST /api/flash/selftest`) | update component (`esp/flash_check.cpp`) |
| `net/wifi_chip_probe` | string: the probe's answer ("running...", or what the co-processor replied) | net service | web UI (Diagnostics tab) |
| `update/available` | bool | update engine | glance strip (update chip), Updates screen |
| `update/in_progress` | bool: downloading or verifying | update engine | shell (hides strip), Updates screen (full-screen progress) |
| `update/stage` | string: idle, checking, up_to_date, available, check_failed, downloading, verifying, ready_to_reboot, failed | update engine | Updates screen, web UI |
| `update/progress_pct` | int 0–100 | update engine | Updates screen, web UI |
| `update/write_pct` | int 0–100: how much of the image is in flash while the stage is `verifying` ("Writing update"); the screen is lit only between flash stalls | update service | Updates screen |
| `update/write_display` | setting: string `pulse` (default: panel off during the flash writes, lit briefly between slices with the progress) or `lit` (screen kept on while writing: glitches; flash auto suspend crashed the boot on this board, so it is not an option); additive, no schema bump | settings (web UI) | update service |
| `update/auto_restart` | setting: bool, default true: restart by itself 5 s after an update from the Updates screen is installed (web uploads still wait for the Restart button); additive, no schema bump | settings (web UI) | update service |
| `update/restart_pending` | bool: the device is about to restart by itself after an install | update service | Updates screen |
| `update/error` | string: readable reason (incl. HTTP status, TLS / socket errors) | update engine | Updates screen, web UI |
| `update/target_version` | string: version being installed | update engine | Updates screen, web UI |
| `update/available_version` | string; empty = none | update engine | Updates screen, web UI |
| `update/notes` | string: changelog of the available build | update engine | Updates screen, web UI |
| `update/catalog` | string: JSON array of builds for this hardware ({version, channel, released, notes, size, installable, reason}) | update engine | web UI (firmware page) |
| `update/previous_version` | string: app in the spare slot; empty = none | update engine | Updates screen, web UI |
| `update/command` | event: "check", "install:<version>", "previous", "reboot" | Updates screen, web UI | update engine |
| `power/battery_text` | string, e.g. "85%" | power service | glance strip (battery chip) |
| `power/battery_icon` | string: LVGL symbol (level / charging) | power service | glance strip (battery chip) |
| `power/usb_power` | bool | power service | update pre-flight, audio service (plug-in sound on false to true) |
| `power/battery_pct` | int 0–100 | power service | update pre-flight |
| `alarm/next_in_s` | int: seconds to the next alarm; -1 = none | alarm (Home) | update pre-flight |
| `time/synced` | bool: SNTP synced | time service | update pre-flight (TLS needs the clock) |
| `sensor/orientation` | string: "landscape" \| "portrait" | IMU service (LIS2DW12) | shell (400 ms debounce, rotation) |
| `ui/fullscreen` | bool | LyricFlash panel | shell (hides strip) |
| `ui/ambient` | bool: night / ambient clock | power service, Home | shell (hides strip) |
| `alarm/ringing` | bool | alarm (Home) | shell (hides strip) |
| `shell/rotation_locked` | bool | shell | glance strip (rotation-lock chip) |
| `shell/current_panel` | string: panel id | shell | diagnostics, web UI |
| `system/hardware_id` | string, e.g. "waveshare-p4-wifi6-lcd5-rev3" | boot (`app_main`) | Updates screen, web UI |
| `system/firmware_version` | string: running app version | boot (`app_main`) | Updates screen, quick settings ("Software version"), web UI |
| `system/build_info` | string: "<build date> <build time>\n<running slot>  <first 8 hex of the image hash>\nFlash id <JEDEC id>, <size> MB" | boot (`app_main`, after the placeholders) | quick settings ("Build") |
| `ui/swipe_lock` | bool: the shown panel takes horizontal swipes itself | shown panel (Recipes, while a recipe is open) | shell (no page swipe) |
| `ui/keep_awake` | bool: keep the screen on (30 min safety timeout) | shown panel (Recipes, while a recipe is open) | power service |
| `input/double_tap` | event: bool, double-tap on the case | IMU service (LIS2DW12) | Recipes (next step / page) |
| `input/activity` | event: bool, a touch on the screen | shell | power service (restarts the idle-sleep time) |
| `sensor/face_down` | bool: lying face-down | IMU service (LIS2DW12) | power service (sleep after 30 s) |
| `power/sleep_blocker` | string: why the device is not sleeping (active, keep_awake, updating, charging, face_down_usb, ringing, due_soon, disabled); empty = going to sleep | power service | diagnostics |
| `power/dnd` | bool: face-down for 30 s; chimes muted, alarms and timers still ring | power service | audio service (hardware, later), diagnostics |
| `power/screen_off` | bool: face-down for 30 s and nothing ringing; screen should be off | power service | display driver (hardware, later), diagnostics |
| `power/wake_cause` | string: power_on, timer, motion, button | power service | shell / services (fast path after wake), diagnostics |
| `recipes/cooking` | string: title of the open recipe; empty = none | Recipes panel | web UI, diagnostics |
| `timers/start` | event: string "<seconds>\|<name>" | Recipes panel | timer service |
| `timers/cancel` | event: int timer id | Recipes panel | timer service |
| `timers/dismiss` | event: bool, stop every ringing timer | Recipes panel | timer service |
| `timers/chip_text` | string: "12:34", "12:34 +2", "Timer done"; empty = no timers | timer service | glance strip (timer chip) |
| `timers/list` | string: one line per timer, soonest first: "<id>\|<remaining_s>\|<0/1 ringing>\|<name>" | timer service | Recipes panel (timers bar), update pre-flight (not empty = blocked) |
| `timers/ringing` | bool: a finished timer is waiting to be stopped | timer service | audio service (alarm sound) |
| `recipes/cooking_chip` | string: short recipe title while a recipe is open on another page; empty otherwise | Recipes panel | glance strip ("Cooking now" chip) |
| `recipes/library_rev` | int: goes up when recipes were deleted | Recipe module (Delete recipes) | Recipes panel (closes the open recipe, reloads the library) |
| `time/unix` | int: unix time, once synced | time service | Recipes ("recently cooked") |
| `storage/sd_present` | bool | storage service | Modules screen (requirements), module panels |
| `storage/sd_free_mb` | int: free space on the SD card | storage service | Modules screen (requirements) |
| `diag/warnings` | string: newline-separated warnings, e.g. a missing add-on; empty = none | health gate (add-on checks) | web UI diagnostics |
| `diag/metrics` | string: `key=value` lines (LVGL busy %, peak fps, panel strips/s, worst LVGL pass ms, heap, PSRAM, uptime, pending_verify, reset reason, and the JPEG decoder: `jpeg_state` (ready/off), `jpeg_decodes`, `jpeg_fails`, `jpeg_last_ms`, `jpeg_last_size`, `jpeg_last_error`, `flash_selftest`, `flash_last_verify`), refreshed every 5 s | UI loop (app_main) | web UI diagnostics, `diag:` log line every 30 s |
| `system/settings_notice` | string; empty = none | boot (`app_main`), shell (clears after reset) | shell (toast + quick settings), web UI |
| `notify/toast` | event: string | any service or panel | shell (toast) |
| `ui/brightness_live` | event: int 0–100, the brightness slider while it is being dragged (not saved; the setting is saved when the finger lifts) | shell (quick settings) | display driver (backlight) |
| `display/blank` | event: bool, true switches the backlight off (whatever the brightness setting), false restores it. Used around a firmware flash write and the health-gate confirm write, which stall the panel | update service | display driver (backlight) |
| `display/panel_off` | event: bool, true sends the panel its display-off command, false display-on. Used only around the firmware flash write, so the starved panel shows no glitching image even with the backlight off | update service | display driver (panel) |
| `ui/volume_live` | event: int 0–100, the volume slider while it is being dragged (not saved) | shell (quick settings) | audio service (`firmware/services/audio`) |
| `audio/play` | event: string `preview` (tick), `warning`, `usb` (plug-in) or `alarm` (cowbell); plays a built-in sound at the saved volume (volume 0 silences all but the alarm; do-not-disturb silences usb; the alarm always rings, never below 40%, and stops when dismissed). The audio service also plays `warning` on `notify/toast_critical` (not while a timer alarm rings) and `usb` when `power/usb_power` goes false to true; nothing else makes a sound | web UI Diagnostics (test buttons), later the shell | audio service |
| `audio/test` | event: same names as `audio/play`, or a library sound id (`kitchen`, `summit-cowbell`, ...: plays that sound once in full); sent by the web Diagnostics test buttons (`POST /api/audio/test`). Ignores do-not-disturb; a test `alarm` plays the first 3 s of the cowbell once without a timer ringing (volume rules otherwise as for `audio/play`) | web UI Diagnostics | audio service |
| `audio/timer_sound`, `audio/alarm_sound` (settings) | string choices = the library ids (timers: kitchen, airplane, marimba, levelup, chisper; alarms: summit-cowbell, beep-beep-beep, hi-low, old_timey, qwarky, ytv); defaults `kitchen` and `summit-cowbell`. A ringing timer plays the `audio/timer_sound` loop back to back until dismissed (the loops carry their own gap); `audio/alarm_sound` is for the alarm feature, not built yet | settings | audio service |
| `notify/toast_link` | event: string "<screen id>\n<text>"; tapping the toast opens that screen, e.g. `recipes/open/<id>` | recipe web API (after a URL import) | shell (toast) |
| `ui/open_request` | string: the screen the shell was asked to open by a tapped toast; the target panel clears it when it takes it | shell | the target panel (read when it is shown) |
| `notify/toast_critical` | event: string | alarm, power service | shell (toast, shown even in full-screen), audio service (warning sound) |
| `ui/open_screen` | event: string screen id: "settings/updates", "settings/modules", "recipes/open/<id>" | shell (update chip, quick settings) | shell (opens the screen), recipes panel |
| `settings/<key>` | event: the new value | settings (on every change) | shell (`settings/ui/page_order`), update engine (`settings/update/channel`: re-check), services |
| `settings/reset` | event: bool | settings | services, UI |

### Path to an app installer

| Stage | What an "app" is | Risk |
| --- | --- | --- |
| v1.0 | Compiled-in panels registered at boot | None |
| v1.x | Widget packs: JSON layout + data bindings, from microSD or URL | Low — no code runs |
| v2 | Sandboxed WebAssembly apps (e.g. WAMR) with a narrow API | Medium — prototype after v1 |

## Updates and recovery

Three ways to update from v0.1: on-device Updates screen (primary), web UI firmware page (backup), USB web flasher (recovery). The on-device screen and web page share one update engine.

```
On-device screen ─┐
                  ├─▶ Update engine (checks, then downloads) ─▶ Write spare slot ─▶ Reboot ─▶ Healthy within 40 s?
Web UI page ──────┘                                                                  yes ─▶ Confirm, What's new card
                                                                                     no  ─▶ Roll back, old slot boots
USB web flasher ─▶ Flash over cable (full, or keep settings)
```

### Carried over from LyricFlash (github.com/skiiirt1983/LyricFlash)

| Pattern | How LyricFlash does it | Summit |
| --- | --- | --- |
| Two app slots + rollback | `ota_0`/`ota_1`, rollback-enabled bootloader | Keep |
| Post-boot health gate | Pending image needs 3 healthy samples 2.5 s apart (UI heartbeat, frames drawn, ≥16 KB heap) within 40 s, else rollback; no internet required (`main/ota_guard.c`) | Keep; add touch + I2C + settings-load checks |
| Fail-fast startup | Any init failure on a pending image triggers rollback | Keep |
| Web upload | Streams `.bin`; header, size, checksum verified before boot slot switch | Keep |
| Install from GitHub | Reads `catalog.json` on Pages; device downloads via HTTPS with URL allow-list, waits for NTP, checks project name + version | Keep |
| Private repo, public updates | CI copies approved OTA images to public Pages | Keep |
| Readable errors | Status endpoint with stage, bytes, HTTP/TLS/socket codes | Keep |
| USB keep-settings | "Preserve" ESP Web Tools manifest: app + otadata reset, NVS untouched (`tools/create_preserve_usb.py`) | Keep |
| Full + factory recovery | Merged full image + Waveshare factory restore page | Keep |
| Release discipline | Immutable per-commit prereleases, `safe/*` branches for hardware-tested builds | Keep |

### Summit additions

1. On-device Updates screen: version, channel, Check now, changelog, Install, full-screen progress, What's new card.
2. One-tap previous version from the spare slot.
3. Pre-flight: USB power or ≥40% battery, no alarm within 15 min, no recipe timer running ("Finish or cancel your timers first"; also blocks going back to the previous version), Wi-Fi + synced time, image fits slot.
4. Catalog carries SHA-256, hardware ID (`waveshare-p4-wifi6-lcd5`) and partition-layout version. Signed catalogs (ed25519) in 1.x.
5. Settings export/import (JSON) and schema-versioned settings with migrations. Adding a key with a default needs no version bump: older firmware ignores unknown keys and preserves them on write. Renames, removals, type or meaning changes bump the version and need a migration + host test. Settings from a newer version are read-only with a notice on device and web UI; reset only by explicit user action after the old settings are saved for export. A corrupt blob runs on defaults in memory, keeps the raw blob for export, and offers reset.
6. `factory` recovery app (Wi-Fi + web upload) boots if both main slots fail.

### Update catalog (format 1)

One `catalog.json` per source, served over HTTPS from GitHub Pages. Stable and beta builds come from the `slopeform-home` catalog; alpha builds from the GitHub Pages of this repository (made public by Paul, 2026-11-22; replaces the separate `summit-alpha` repo), published by `.github/workflows/release.yml`:

- Stable / beta: `https://skiiirt1983.github.io/slopeform-home/catalog.json`
- Alpha: `https://skiiirt1983.github.io/project-summit/catalog.json` (OTA images under `/firmware/<version>/`; the `gh-pages` branch keeps every published version)

```json
{
  "format": 1,
  "project": "summit",
  "builds": [
    {
      "version": "0.1.0",
      "channel": "stable",
      "hardware_id": "waveshare-p4-wifi6-lcd5-rev3",
      "partition_layout": 1,
      "url": "https://skiiirt1983.github.io/slopeform-home/firmware/0.1.0/summit-0.1.0-rev3-ota.bin",
      "size": 1228800,
      "sha256": "64 lowercase hex characters",
      "released": "2026-11-07",
      "notes": "Short changelog shown on the Updates screen"
    }
  ]
}
```

Device rules (`firmware/services/update`, host-tested):

- The whole catalog is ignored unless `format` is 1 and `project` is `summit`. Unknown fields are ignored.
- A build is installable only if: `version` is SemVer (`MAJOR.MINOR.PATCH[-pre]`), `hardware_id` equals the device's (board + chip-revision family), `partition_layout` equals 1, `url` is `https://` on an allowed host (`skiiirt1983.github.io`) with no credentials or port, `0 < size ≤` the 8 MB slot, and `sha256` is 64 lowercase hex.
- Channels: a device on stable sees stable builds; beta sees stable + beta; alpha sees all. "Update available" = the newest installable build newer than the running version (SemVer precedence). Older installable builds stay selectable (deliberate downgrade).
- Downloads: HTTPS with the certificate bundle, redirects disabled (the allow-list applies to the final host), clock synced first. The first 0x70 bytes are checked before anything is written: ESP32-P4, matching chip-revision range, project `summit`, version equal to the catalog entry. Size and SHA-256 must match; ESP-IDF then validates the image before the spare slot is selected.
- One update at a time: on-device installs and web uploads share the engine. Uploads skip the catalog checks (no SHA-256 to compare) but get the same header, size and image validation.
- Health gate after reboot: 3 consecutive healthy samples 2.5 s apart within 40 s (UI heartbeat ≤ 1.5 s old, frames drawn, ≥ 16 KB internal heap, touch controller answers, the built-in I2C devices answer — GT911 touch, ES8311 codec, ES7210 —, settings loaded without an invalid schema); otherwise rollback. Add-ons (LIS2DW12 accelerometer, microSD) never fail the gate: a missing add-on is logged as a diagnostics warning (`diag/warnings`). Any init failure on a pending image rolls back at once. "Previous version" asks for confirmation ("Go back to vX? Settings from the newer version may become read-only."), then boots the spare slot as a pending image, so it goes through the gate too. After the gate confirms an update, an "Updated to X" toast is shown.

### Update channels

| Channel | Who | Source | Auto-check |
| --- | --- | --- | --- |
| Stable | Everyone | `slopeform-home` releases | Daily, notify only |
| Beta | Opt-in testers | `slopeform-home` prereleases | Daily, notify only |
| Alpha | Paul | Binaries-only public repo fed by `project-summit` CI | On demand |

### Partition layout (32 MB flash) — fixed in v0.1, never changed

| Partition | Size | Purpose |
| --- | --- | --- |
| nvs | 1 MB | Settings, Wi-Fi, alarms |
| otadata + phy | 12 KB | Boot slot record, radio calibration |
| factory | 3 MB | Recovery app |
| ota_0 | 8 MB | Main app slot A |
| ota_1 | 8 MB | Main app slot B |
| coredump | 64 KB | Crash dump |
| assets | ~11 MB (rest) | LittleFS: fonts, sounds, web extras |

Confirm the first build leaves ≥30% headroom in an 8 MB slot.

**Boot gotcha:** with a `factory` partition present, a blank (erased) `otadata` makes the bootloader start the factory recovery app, not the main app. The main app always lives in `ota_0`, never `factory`. Every USB image (full install and keep-settings) must write an initialised `otadata` that selects `ota_0`. LyricFlash's keep-settings manifest resets otadata to blank; that works for LyricFlash (no factory partition) but would boot Summit into recovery.

**Chip revision:** ESP32-P4 v1.x and v3.x chips need different firmware builds, selected by `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` (`y` = v1.x, `n` = v3.x, default). ESP-IDF 5.5.3 or later is required for v3.x. A build for one family will not boot on the other. CI builds both variants; each catalog entry and the hardware ID carry the revision family, and the update engine installs only the matching variant. Paul's board revision is confirmed from the boot log in Phase 1. ([Espressif v3.x guide](https://documentation.espressif.com/esp32-p4-chip-revision-v3.x_user_guide_en.html))

### USB recovery tiers

1. Update over USB, keep settings. 2. Full install (erase). 3. Waveshare factory restore.

## Panels for v1

| Panel | Data source | Where it runs | Landscape | Portrait |
| --- | --- | --- | --- | --- |
| Home (clock + alarm) | SNTP + RTC; alarms in NVS | Device | Big clock left, agenda + weather right | Clock top, agenda below |
| Calendar | Google Calendar secret iCal (ICS) URL | Device | Week view | Day list |
| Weather | Open-Meteo (no key) | Device | Now + hourly + 7-day | Stacked |
| News | User RSS/Atom feeds | Device | 3-column cards | Single column |
| LyricFlash | Mic + LyricFlash service | Visuals on device; ID via service | Full-bleed visual, lyric overlay | Visual top, lyrics below |
| Printer | Bambu MQTT (LAN/cloud/hybrid), PrintSphere port | Device | Ring + temps + AMS + snapshot | Ring top, details below |

- **Clock / alarm:** alarms ring on battery; device drops to ambient clock rather than powering off. Snooze = double-tap the case; dismiss = swipe.
- **Calendar:** private ICS links pasted into web config. Event editing (Google sign-in on device) parked for 1.x.
- **Weather:** check Open-Meteo terms before 1.0. Sunrise/sunset drives night mode.
- **News:** feed thumbnails only, size cap (~300 KB), P4 hardware JPEG decode, streaming XML parse.
- **LyricFlash:** on-device FFT visuals always; recognition goes device → LyricFlash HTTPS endpoint; panel contract `{title, artist, album, art_url, duration, position_ms, lyrics: [{t_ms, line}], mood}`; Song info sheet with QR.
- **Printer:** port PrintSphere's client + status parsing (licence permitting); new UI; print-done/HMS as system notifications.

### Pager

One panel per page, horizontal swipe. Home is always the first page; the order of the rest comes from the `ui/page_order` setting (set in the web config).

### Glance strip

Motion for the pager, the sheet and the system screens is described in "Motion and controls" below.


- Thin bar on the top edge of every panel, both orientations. Height = one size token (~6% of the short side, ~44 px). Panels get the content area below it and never draw under it.
- Contents, left to right: time · next calendar event (title + countdown) · weather now (icon + temp) · printer progress (only while printing) · right side system icons: Wi-Fi, update available, rotation lock, battery % / charging.
- Each item is a "glance chip" fed by a state-store topic. Panels can register a chip in their manifest, so new panels add chips without shell changes. A chip with no data is hidden.
- Narrow width (portrait): drop chips in this order: printer, weather, next event. Time and battery never drop.
- Gestures: tap a chip → go to its panel; swipe down from the strip → quick-settings sheet (brightness, volume, rotation lock, do not disturb); long-press → rotation lock.
- System chip taps: time → Home; update → Settings → Updates; rotation lock → turns the lock off; Wi-Fi and battery → quick-settings sheet.
- Notifications (print done, update available, etc.) briefly expand the strip as a toast, then collapse. Toasts wait while the strip is hidden, except critical ones (alarm, low battery), which show immediately even in full-screen. Duplicates merge into one; queued toasts expire after 10 min.
- Hidden during: LyricFlash full-screen visuals, ringing alarm, night/ambient clock, update progress (tap reveals it for 3 s).

### Motion and controls

Designed 2026-11-16 at Paul's request; built for the simulator, **not yet seen on the panel** (frame rate and touch feel are Phase 1 items in HARDWARE_TODO.md). The aim is a phone-like feel: everything on screen answers the finger at once and moves the same way everywhere.

**Rules**

1. **The finger is in charge.** A dragged page sits under the finger, 1:1. Letting go continues from where it is at the finger's speed; nothing restarts or jumps.
2. **Something always answers within 80 ms.** Every button and chip shrinks a little and lightens the moment it is touched, then relaxes.
3. **Arrive soft, leave quick.** Things coming in decelerate into place; things going out take about a quarter less time and speed up as they go.
4. **Move, don't fade.** Screens and sheets travel from where they live (the sheet from the strip, settings pages from the right), so it is clear where to send them back. Only cheap properties are animated (position and background opacity), never whole-screen opacity or scaling, so the software renderer can keep up.
5. **One set of numbers.** All durations and curves are tokens in `summit/shell/motion.h` (pure, host-tested); the LVGL helpers are in `summit/shell/ui_motion.h`. Panels use the helpers instead of writing their own animations.

**Tokens**

| Token | Value | Used for |
| --- | --- | --- |
| Press / release | 80 ms / 200 ms | Buttons and chips: shrink by `space_s / 4` a side and lighten 20% |
| Page settle | up to 320 ms, at least 140 ms | Page swipe and snap-back; a flick is shorter (2 × distance left ÷ finger speed) |
| Sheet in / out | 300 ms / 220 ms | Quick settings drops from behind the strip while the page dims; goes back up |
| Screen in / out | 320 ms / 240 ms | Updates and Modules are pushed in from the right and pulled back out |
| Page dots | hold 1.2 s, fade 0.3 s | Which page of how many; shown while swiping and just after |
| Enter curve | cubic-bezier(0.05, 0.7, 0.1, 1) | Anything arriving |
| Exit curve | cubic-bezier(0.3, 0, 0.8, 0.15) | Anything leaving |
| Standard curve | cubic-bezier(0.2, 0, 0, 1) | Moves that start and end on screen (dimming) |

**Behaviour**

- **Page swipe:** both pages travel together, edge to edge. Only one panel holds LVGL objects at a time, so the outgoing page leaves as a picture of itself (LVGL snapshot, about 1.7 MB in RGB565, freed when the move ends). If there is no memory for the picture the new page still slides in alone.
- **Ends of the page list:** dragging past the first or last page stretches like a rubber band (about half the finger's travel at first, then less) and springs back.
- **Chip taps and "go to panel"** use the same page move, in the direction of the target page.
- **Buttons:** one primary action per screen in the accent colour; other actions on the chip colour; destructive actions in red text and never the first button of a dialog. Touch targets are at least one size token high.
- **Not yet built** (next motion steps): recipe step change and library → recipe as moves, dialogs rising from the bottom, toast slide, rotation morph with cards sliding to their new places.

## Modules and Recipe reader

Decided in DECISIONS.md D-008; built in steps R1–R7 (see `docs/KICKOFF.md`). This section is extended as each step lands.

### Modules (v1 "installable")

- v1 modules ship inside the firmware but start uninstalled. Downloadable code modules stay a v2 item (WASM).
- A **Modules** screen on the device and in the web UI lists each module: name, icon, short description, requirements, Install / Remove.
- Each module has a manifest (`firmware/framework/include/summit/modules.h`): id, name, icon, description, the panel it provides, and requirements (`sd_card`, `min_free_mb`). Install checks them and explains what is missing ("Insert a microSD card").
- Install = setting `modules/<id>/installed` = true (additive key, no schema bump) and the module's panel joins the pages (after the existing ones unless `ui/page_order` lists it). Remove = hidden from pages; its data on the SD card is kept.
- **Delete recipes (D-010)** is a separate button, never part of Remove. It is offered whenever the card holds recipes (module installed or not) and shows the count ("Delete 12 recipes"). It asks first, naming the count: "Delete 12 recipes from the SD card? This can't be undone." (Cancel / Delete). It removes every `/recipes/<id>/` folder (including ones that could not be read) and rebuilds the index; PDFs waiting in `/recipes/inbox/` are kept. An open recipe is closed (`recipes/library_rev`). Same button and same question on the device's Modules screen and in the web UI.
- If the SD card is removed later the module stays installed; its panel shows "Insert the SD card with your recipes".
- Web API: `GET /api/modules`, `POST /api/modules/install`, `POST /api/modules/remove` (`{"id": ...}`). `GET` also gives `data_count`, `data_noun` and `delete_question`. `POST /api/modules/delete-data` needs `{"id", "confirm": true, "count": N}` where N is the count the user was shown; if the count has changed it is refused (409) and nothing is deleted.

### Recipe reader (summary of D-008)

- Two modes: **card mode** (parsed recipe: ingredients + one big step at a time) and **book mode** (original PDF pages as images).
- No PDF parsing on the device: the web UI converts PDFs in the browser with pdf.js (pages → JPEG, text → draft recipe, edit screen), from an upload or from `/recipes/inbox/` on the SD card. A pasted recipe URL is fetched by the device and read from its schema.org `Recipe` JSON-LD.
- Storage: `/recipes/<id>/recipe.json` (format 1), `pages/NNN.jpg`, `photo.jpg`, `original.pdf`; `/recipes/index.json` is a rebuildable index.
- Kitchen features: library with search/tags, step timers (glance chip, several at once), servings scaling with fractions, tick off ingredients, double-tap the case for next step/page, screen stays on while cooking (30 min safety timeout), "Cooking now" chip.

### Recipes panel

- **Library:** recipe cards (recently cooked first), search by title, tag filter buttons, and a notice with a QR code to the web UI when `/recipes/inbox/` holds PDFs. Without a card: "Insert the SD card with your recipes".
- **Card mode:** ingredients (left in landscape, above in portrait) with servings − / + and tap-to-tick; one big step at a time with Back / Next, and a timer button for each time found in the step.
- While a recipe is open the panel takes horizontal swipes (left = next step, right = previous; `ui/swipe_lock`), keeps the screen on (`ui/keep_awake`) and reports `recipes/cooking`. A double-tap on the case (`input/double_tap`) is Next. Leaving the page restores normal swiping and dimming; the recipe stays open on its step.
- **Book mode:** the original pages as images, one at a time: Back / Next, swipe, or a double-tap on the case. Fit = full width in landscape (scroll down to read on) and the whole page in portrait; the Zoom button or a double-tap on the page doubles it and dragging pans. A recipe with steps and pages has "Pages" / "Steps" buttons to switch; a scan (pages only) opens straight in book mode.
- The panel reads recipes through `RecipeStore` (the storage interface) and gets page images through an image-source function, both given to it when it is created. JPEG decoding is the platform's job: libjpeg-turbo in the simulator; on the device `firmware/services/jpeg` registers an LVGL image decoder for the ESP32-P4 hardware JPEG engine (baseline JPEGs only, output RGB565 in PSRAM with sizes rounded up to 16, kept in LVGL's 8 MB image cache; `jpeg_info.cpp` reads the header and plans memory and is host-tested). LVGL's POSIX file driver maps `S:` to `/sdcard`. A progressive or oversized JPEG fails with a reason (`jpeg_last_error`) and the panel shows its images-unavailable message. Compiled in CI; not yet run on the board.

### Recipe import in the web UI

The **Recipes** tab of the web UI (`firmware/web/page/recipes.mjs`) does the conversion in the browser; the device never parses a PDF.

1. Choose a PDF, or press **Convert** next to a PDF waiting in `/recipes/inbox` (the browser fetches it from the device).
2. pdf.js (4.10.38, Apache-2.0, vendored in `firmware/web/page/vendor/pdfjs/` and served by the device) renders every page to a JPEG with its long side at 1280 px, plus a 320 px thumbnail of page 1, and extracts the text.
3. `recipe_import.mjs` turns the text into a draft: title, servings ("Serves 8", "Makes 4"), ingredients under an *Ingredients* heading (quantity incl. fractions like `1 1/2` and `½`, unit, item, note), and steps under *Method / Directions / Instructions / Steps* (numbered lines, or sentences when unnumbered). A PDF with several recipes offers a choice. No heading found (a scan): pages only.
4. The **edit screen** shows the draft (title, servings, tags, ingredients and steps one per line) with page previews. Nothing is saved until **Save to device**.
5. Saving posts the recipe (validated by the device), uploads the pages and thumbnail, and keeps the PDF with the recipe as `original.pdf` (an inbox PDF is moved there).

The web files are gzipped into the firmware image (about 510 KB for pdf.js) and served from flash.

### Recipe import from a website

- In the web UI's Recipes tab, paste a recipe page's address. `POST /api/recipes/import-url` makes the **device** fetch the page and read its schema.org `Recipe` JSON-LD block (`firmware/panels/recipes/src/jsonld.cpp`, host-tested on the sample pages in `tests/fixtures/web/`).
- Understood shapes: a plain `Recipe` object, one inside `@graph` or an array, `@type` as a list; `recipeYield` as a number, text or list; `recipeIngredient` lines (parsed into quantity / unit / item / note like the PDF import); `recipeInstructions` as text, `HowToStep` or `HowToSection`; `image` as a URL, list or `ImageObject`; `keywords` and `recipeCategory` become tags. HTML tags and entities are cleaned.
- Result: a card-mode recipe (no pages) with `source = {type: "url", url}`, plus the main photo as `photo.jpg` when the page names one over https and it is a JPEG. It is saved directly (no edit screen yet). After the import the web UI highlights the new recipe (with Edit). The device never jumps to it (someone may be cooking): it shows a short toast "New recipe: <title>" (`notify/toast_link`) and opens the recipe only if the toast is tapped.
- Only public `https://` addresses are fetched: no IP addresses, `localhost`, single-label or `.local` / `.lan` names, ports or credentials; redirects are re-checked. Pages are capped at 2 MB.
- The device-side fetcher (`firmware/panels/recipes/esp/url_fetch.cpp`) is written but **untested on hardware**; the simulator uses the sample pages instead of the internet.

### Recipe web API (for import in the browser)

Provided by the Recipe reader as an extension of the web API (`firmware/panels/recipes/src/web_api.cpp`, host-tested). Nothing here deletes a recipe (deleting all recipes is `POST /api/modules/delete-data`, see Modules).

| Endpoint | Purpose |
| --- | --- |
| `GET /api/recipes` | library index, inbox PDF names, folders that could not be read |
| `GET /api/recipes/get?id=` | a recipe's `recipe.json` |
| `POST /api/recipes/save` | `{"id"?, "recipe": {...}}`: validated with the device's parser; a new id is made from the title when `id` is absent. With `id` it replaces that recipe's text (the Recipes tab's Edit button); pages and photo are untouched |
| `POST /api/recipes/file?id=&name=` | streamed upload (header `X-Summit-Upload: 1`) of `pages/NNN.jpg` (1–200), `photo.jpg`, `thumb.jpg`, `original.pdf`; images up to 2 MB, PDFs up to 32 MB; nothing is kept from an incomplete upload |
| `GET /api/recipes/file?id=&name=` | those files back, streamed from the card |
| `GET /api/recipes/inbox?name=` | a PDF waiting in `/recipes/inbox/` |
| `POST /api/recipes/inbox/done` | `{"name", "id"}`: moves the inbox PDF into the recipe as `original.pdf` (kept for re-conversion) |
| `POST /api/recipes/import-url` | `{"url"}`: the device fetches the page and saves its schema.org Recipe (and photo) |

Large bodies are streamed in both directions (`StorageWriter`, `read_chunk`), never held whole in RAM. Without a card every endpoint answers 409 "Insert a microSD card".

### Kitchen timers

- `firmware/services/timers`: up to 8 named timers at once (1 s to 48 h); a timer without a name is called "Timer 1", "Timer 2" ... (the lowest number not in use). Also pure logic, not yet wired to the bus or a screen (design: `docs/TIMER_BRIEF.md`): `Stopwatch` (start/stop/lap/reset, laps kept until reset, up to 99) and `Alarms` (up to 8 alarms with a time of day, repeat days, snooze; works on local wall-clock seconds; a once-alarm switches itself off; late alarms after sleep are skipped). Tapping a time in a step raises `timers/start`; the service counts down and publishes `timers/chip_text`, `timers/list` and `timers/ringing`.
- When a timer ends it raises a critical toast ("Timer done: <name>", shown even in full-screen modes) and rings until stopped; if nobody stops it, it stops by itself after 1 minute.
- **Glance chips** (declared in the Recipes panel's manifest): a timer chip with the soonest countdown (never dropped at narrow widths) and a "Cooking now" chip with the recipe title while a recipe is open on another page. Tapping either returns to the Recipes page.
- The Recipes panel lists running timers above every view with Cancel, and Stop for a finished one.

### Storage layer

- `summit::Storage` (`firmware/framework/include/summit/storage.h`) is the only way code reaches the SD card: `present`, `free_mb`, `exists`, `read`, `write` (temp file + rename, parents created), `list`, `make_dir`, streaming `begin_write` / `read_chunk`, `size`, and `move` (never overwrites). The only delete is `remove_tree`, used by "Delete recipes": it refuses the card root and top-level folders, so it can only remove something inside a folder such as `/recipes/`. Paths are absolute within the card and validated (no `..`).
- `PosixStorage` implements it over a mounted folder: `/sdcard` on the device, a local folder in the simulator and host tests (the "fake SD card").
- `RecipeStore` (`firmware/panels/recipes`) implements the `/recipes/` layout on top: load / rebuild the index, load / save a recipe, new ids, mark cooked, list the inbox PDFs.
- The device SD service (`firmware/services/storage`, untested on hardware) mounts the card without ever formatting it, re-checks every 3 s, and publishes `storage/sd_present` and `storage/sd_free_mb`.

### `recipe.json` format 1

Code: `firmware/panels/recipes` (`recipe.h`, `quantity.h`, `step_time.h`), host-tested in `tests/host/test_recipes.cpp`.

```json
{
  "format": 1,
  "title": "Pancakes",
  "servings": 4,
  "ingredients": [
    {"qty": 1.5, "unit": "cup", "item": "flour", "note": "sifted"},
    {"qty": null, "unit": "", "item": "salt", "note": "to taste"}
  ],
  "steps": [{"text": "Rest the batter for 30 minutes.", "timer_s": 1800}],
  "source": {"type": "pdf", "name": "pancakes.pdf"},
  "tags": ["breakfast"],
  "created": "2026-11-12T09:00:00",
  "cooked": 1765000000
}
```

- `format` and `title` are required. Unknown fields are ignored; a `format` above 1 is refused ("newer format"). Limits: 300 ingredients / steps / tags, 4000 characters per text.
- `qty` is a number or null; ingredients without a number are never scaled. `source.type` is `pdf`, `url` or `manual`. `cooked` (unix time, optional) is when the recipe was last opened for cooking.
- Card mode exists when there is at least one ingredient and one step. Book mode exists when the folder has `pages/`.
- Recipe ids (folder names): lowercase letters, digits and `-`, up to 48 characters, made from the title; `inbox` is reserved.
- **Scaling:** quantity × (wanted servings ÷ recipe servings). Non-metric quantities show as fractions (halves, thirds, quarters, eighths when within 0.02, e.g. `1 1/2 cup`); metric units (g, kg, mg, ml, cl, dl, l) show decimals (`375 ml`, `1.25 kg`).
- **Step times:** found in the step text for tap-to-start timers: `60 minutes`, `25–30 minutes` (range → lower bound), `10 to 12 min`, `1 hour 30 minutes`, `1 1/2 hours`, `30-minute`, `45 sec`. Numbers without a time unit (350 degrees, 3 eggs) are ignored.
- **Index** `/recipes/index.json`: `{format: 1, recipes: [{id, title, tags, has_card, has_pages, has_photo, pages, cooked}]}`, recently cooked first, then by title. If it is missing or corrupt it is rebuilt from the folders; folders with a bad name or unreadable `recipe.json` are skipped and reported.

## Sleep (D-012)

On battery the device sleeps whenever nothing needs it (screen off, Wi-Fi off, ESP32-P4 deep sleep) and wakes itself in time for timers and alarms. The rules are pure code in `firmware/services/power/` (`summit/sleep.h`), host-tested in `tests/host/test_sleep.cpp`; the deep-sleep call sits behind `SleepPort` and is untested until Phase 1.

- **Enter sleep:** no touch for the idle timeout (setting `power/idle_sleep_min`, default 5 min; 0 = never), or face-down for 30 s. `PowerService` (`summit/power_service.h`) checks once a second.
- **Never while:** a recipe is open with the screen-on rule (`ui/keep_awake`), an update runs, a timer or alarm is ringing, or on USB power with "stay on when charging" on (setting `power/stay_on_charging`, default on). These hold face-down too. **Face-down on USB** (any "stay on" setting, nothing ringing or updating): the screen turns off and sounds are muted (`power/screen_off`, `power/dnd`), but the device never deep-sleeps, so Wi-Fi and data stay fresh (blocker `face_down_usb`). On battery, face-down still sleeps after 30 s. The screen-on rule itself ends after 30 minutes without a touch, so a forgotten open recipe cannot drain the battery.
- **Before sleeping:** running timers (name, length, time left) and the next alarm are written to RTC memory as one small blob (magic number + CRC-32, at most 1 KB; a damaged blob is ignored). The RTC timer is set for the earliest due item minus the wake margin (default 5 s, to be measured on the board). If something is due within the margin the device stays awake for it.
- **Wake sources:** RTC timer (timer/alarm due), LIS2DW12 INT1 (pick-up / double-tap), power button. The cause is published as `power/wake_cause`.
- **After wake, fast path:** timer/alarm wake → screen + audio and ring before Wi-Fi starts. Pick-up / button wake → the clock at once, Wi-Fi and data refresh in the background. Panels show the last data with its age rather than a blank page.
- **Timers across sleep:** restored with the time asleep taken off; one that ended during sleep rings at once. Across a full power loss they are still lost until the external RTC is proven (D-011).
- **Wi-Fi while asleep:** none. A periodic refresh wake is a possible later option, off by default.

## "Oh nice" features

| Feature | Trigger | What happens |
| --- | --- | --- |
| Pick-up wake | Accelerometer wake | Screen fades up before touch |
| Morphing rotation | 4D change | Cards slide to new places |
| Rotation lock | Long-press status bar | Pins orientation |
| Face-down DND | 6D face-down | Screen off, chimes muted, alarms ring (on USB: no deep sleep) |
| Tap the case | Double-tap | Snooze / play-pause visuals |
| Sunrise alarm | 10 min before alarm | Warm colour ramp |
| Night clock | After sunset | Dim red-amber clock |
| Music noticed | Sustained music on mic | "Now playing?" chip |
| Print-done moment | Printer finished | Edge glow + chime + thumbnail |
| "Leave in 10 min" | Next event with location | Countdown + rain hint |
| Charging glow | USB power | Battery ring animation |
| QR everything | Setup, settings, song info | Scan instead of typing |
| What's new card | After update | Change list |

Parked add-ons: snow report panel, Home Assistant tiles, camera presence wake, pogo-pin charging stand, ambient-light sensor.

## Repositories and release

Private `project-summit` for alpha/beta; fresh public `slopeform-home` seeded from a clean tag at 1.0.

```
firmware/        ESP-IDF project
web-config/      device config portal source
enclosure/       STEP + 3MF per part, dimensions file, print settings
hardware/        wiring diagram, BOM, I2C map, measurements
packs/           example widget packs (v1.x)
docs/            setup guide, architecture, panel API, contributing
.github/         Actions: build, release, flasher page
```

Licensing: firmware GPL-3.0; enclosure files under a Creative Commons licence; PrintSphere port only if its licence is GPL-compatible (else clean-room Bambu MQTT client); LyricFlash engine openness to decide.

## Risks and open questions

| Risk | Mitigation |
| --- | --- |
| Landscape on native-portrait panel drops fps | Week-1 spike: LVGL vs P4 2D-accelerator rotation |
| I2C clash codec ↔ accelerometer | Bus scan, set SA0 |
| Slow charging of 8000 mAh | Measure charge current |
| Mic buried in enclosure | Align mic port, test before final shell |
| PrintSphere licence | Check now; plan B clean-room client |
| LiPo heat / swelling | Air gap, divider, swell allowance, protected pack |

Open: where LyricFlash recognition runs (needs an internet-reachable HTTPS endpoint); which LIS2DW12 breakout; battery model/dimensions; desk-only or wall-mountable.

## Sources

- [Waveshare ESP32-P4-WIFI6-Touch-LCD-5](https://www.waveshare.com/product/arduino/displays/lcd-mipi/esp32-p4-wifi6-touch-lcd-5.htm)
- [PrintSphere on MakerWorld](https://makerworld.com/models/2517189)
- [LyricFlash](https://github.com/skiiirt1983/LyricFlash)
- ESP32-P4-WIFI6 schematic; ST LIS2DW12 datasheet DS11811 Rev 9
