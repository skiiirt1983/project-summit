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
| INT1 | GPIO21 (proposed) | 4D orientation, double-tap, wake — routed to one pin |
| INT2 | GPIO22 (optional) | Spare for sleep-state change |
| CS | Tie to VDD_IO | Forces I2C mode |
| SA0 | Set after bus scan | Picks 0x18 or 0x19 |

GPIO21/22 come from the header's net list in the schematic; confirm against the silkscreen before soldering.

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
3. Pre-flight: USB power or ≥40% battery, no alarm within 15 min, Wi-Fi + synced time, image fits slot.
4. Catalog carries SHA-256, hardware ID (`waveshare-p4-wifi6-lcd5`) and partition-layout version. Signed catalogs (ed25519) in 1.x.
5. Settings export/import (JSON) and schema-versioned settings with migrations.
6. `factory` recovery app (Wi-Fi + web upload) boots if both main slots fail.

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

## "Oh nice" features

| Feature | Trigger | What happens |
| --- | --- | --- |
| Pick-up wake | Accelerometer wake | Screen fades up before touch |
| Morphing rotation | 4D change | Cards slide to new places |
| Rotation lock | Long-press status bar | Pins orientation |
| Face-down DND | 6D face-down | Screen off, chimes muted, alarms ring |
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
