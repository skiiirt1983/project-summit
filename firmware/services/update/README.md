# firmware/services/update — the update engine

One engine for every update path (hard rule 5): the on-device Updates screen and the web UI
both drive it; USB recovery is separate (ESP Web Tools manifests from `tools/package_firmware.py`).

- `src/` (pure, host-tested in `tests/host/test_update.cpp`): SemVer, catalog validation and
  selection, pre-flight, image header check, the engine state machine, the post-boot health gate,
  status topics.
- `esp/` (device): spare-slot OTA writer, mbedTLS SHA-256, HTTPS download (cert bundle, no
  redirects, host allow-list), catalog JSON (cJSON), update service task, health gate task,
  previous-version switch.

Catalog format and rules: `docs/ARCHITECTURE.md` "Update catalog". Release side: `tools/catalog.py`.
