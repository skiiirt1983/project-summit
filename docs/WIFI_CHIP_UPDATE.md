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

## Steps
1. CI: manual workflow `.github/workflows/wifi-chip.yml` builds the C6 slave image (artifact) and prints the host-side
   update API from the real headers. *(this change)*
2. Device code against those headers: a development-only upload on the web UI (no catalog yet) that streams a `.bin`
   to the C6 and reports progress; plus the co-processor version on the Build row.
3. Probe on the board, then (only with Paul's explicit OK) the real update; confirm the version mismatch warning is
   gone, then restore the plain/fast SDIO link settings (see HARDWARE_TODO).
4. Design decision for Paul (rule 10): how the C6 image reaches users (GitHub Pages catalog entry next to the P4
   image, an "Update Wi-Fi chip" button on the Updates screen, same update engine and health-gate pattern).
