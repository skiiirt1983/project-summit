# Decisions from Paul (via the Claude design session)

Newest first. Each entry answers a question Claude Code asked. Apply it, update ARCHITECTURE.md where noted, then continue.

## D-007 — Step 6 web UI: import, labels, Wi-Fi setup, web UI lock (2026-11-07)

1. **Settings import = replace all.** Approved, with guards:
   - Validate the whole file first. Newer schema version or invalid → refuse with a clear message, change nothing.
   - Confirm before applying: "Replace all settings? Your current settings are backed up."
   - Keep the backup downloadable as "Undo import".
   - Export leaves out secrets by default (Wi-Fi password, printer access code, tokens), with a tick-box to include them. Import keeps current secrets if the file has none.
2. **Settings labels.** Raw keys are fine for alpha. Add optional `label` and `help` fields to the settings schema now (code only, no stored-settings change). Fill them before the first beta.
3. **Wi-Fi setup** is its own hardware step, agreed. The setup access point must use WPA2 with its password in the on-screen QR code. Never an open network.
4. **Web UI lock (new design decision, add to ARCHITECTURE.md).**
   - Read-only pages (status, diagnostics) stay open on the LAN.
   - Any change (firmware upload/install, previous version, settings import/reset) requires pairing.
   - Pairing: the device screen shows a 6-digit code; the browser enters it once and gets a session token valid 30 days, revocable from on-device settings.
   - Build it in this step or the next.
