# Wi-Fi chip (ESP32-C6) firmware update: plan (2026-11-23)

Paul's go-ahead: start. Hard requirement from Paul: **no soldering and no jumpers**; the update goes over the existing
SDIO link from the P4. Status: step 1 (build) in progress. Nothing has been flashed to the C6.

## What is known
- The P4 firmware uses esp_hosted **2.13.x** (host side). The C6 reports firmware **0.0.0** ("Version mismatch: Host
  [2.13.0] > Co-proc [0.0.0]", "did not report transport size"): it runs an older esp_hosted slave.
- The esp_hosted host component has a slave-OTA API (begin / write chunks / end / activate; the activate step needs a
  slave >= 2.6.0, an older slave just restarts into the new image).
- Slave firmware source: `espressif/esp-hosted-mcu`, directory `slave/`, built for `esp32c6` with SDIO.

## The big unknown
Does the firmware shipped on the C6 understand the OTA commands at all? A slave that reports 0.0.0 is old and may not.
If it does not, the host cannot update it, and the only way is a one-time wired flash of the C6 (BOOT button and UART
on the board, as in Waveshare's guides). That is Paul's call: it would break the no-jumpers requirement.
The first on-device test is therefore a harmless probe: start the OTA against the current slave and see whether it
accepts it. A refusal changes nothing.

## Risk
A wrong or broken C6 image leaves the device **without Wi-Fi** (and so without Wi-Fi updates). A USB flash of the P4
cannot repair the C6. Therefore: only an image built from the same esp-hosted-mcu tag family as the host (2.13.x),
flashed only after Paul says so for that specific step.

## Step 1 result (2026-11-23)
- The manual workflow built the slave from `espressif/esp-hosted-mcu` tag **v2.13.1** for esp32c6: `network_adapter.bin`,
  1,212,512 bytes, SHA-256 `69b679e1a3d2c40204035e3b7f965071bb5a76813430cdd3f14b6e896fd462e0`, SDIO streaming mode,
  C6 pins CMD 18, CLK 19, D0-D3 20-23 (the Espressif P4 function board wiring; the P4 side already uses GPIO14-19 and
  works with the C6's current firmware, so the wiring matches). Artifact `wifi-chip-v2.13.1-<sha>` (kept 90 days).
- The host-API job printed nothing useful: my command was quoted twice by the CI action. Replaced by
  `tools/wifi_chip_api.sh` (run again after this change merges).

## API (read from the esp_hosted 2.13.1 host headers, 2026-11-23)
`esp_hosted_ota.h`: `esp_hosted_slave_ota_begin(void)`, `esp_hosted_slave_ota_write(uint8_t *data, uint32_t len)`,
`esp_hosted_slave_ota_end(void)`, `esp_hosted_slave_ota_activate(void)` (reboots the co-processor). `esp_hosted.h`:
`int esp_hosted_get_coprocessor_fwversion(esp_hosted_coprocessor_fwver_t *)`. `esp_hosted_misc.h`:
`esp_hosted_get_coprocessor_app_desc(esp_hosted_app_desc_t *)`. The old URL based `esp_hosted_slave_ota()` is deprecated.

## Step 2 (this change): the probe
Web UI, Diagnostics tab, button "Probe Wi-Fi chip": the net service calls `esp_hosted_slave_ota_begin()` and, if it
answers OK, `esp_hosted_slave_ota_end()` with no data. It never writes firmware and never calls activate. The answer shows
as "Wi-Fi chip probe" in Diagnostics (state `net/wifi_chip_probe`). Wi-Fi may drop for a few seconds while the chip answers.

## Steps
1. CI: manual workflow `.github/workflows/wifi-chip.yml` builds the C6 slave image (artifact) and prints the host-side
   update API from the real headers. *(this change)*
2. Device code against those headers: a development-only upload on the web UI (no catalog yet) that streams a `.bin`
   to the C6 and reports progress; plus the co-processor version on the Build row.
3. Probe on the board, then (only with Paul's explicit OK) the real update; confirm the version mismatch warning is
   gone, then restore the plain/fast SDIO link settings (see HARDWARE_TODO).
4. Design decision for Paul (rule 10): how the C6 image reaches users (GitHub Pages catalog entry next to the P4
   image, an "Update Wi-Fi chip" button on the Updates screen, same update engine and health-gate pattern).
