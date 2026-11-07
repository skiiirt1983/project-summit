# firmware/framework

The app framework, the seam between panels and everything else (ARCHITECTURE.md "Panel contract").
Plain C++17, no ESP-IDF or LVGL dependency, so `tests/run_tests.sh` builds and tests the same sources on the host.

- `event_bus`: any task publishes; the UI task dispatches. Bounded queue, drops oldest when full (counted).
- `state_store`: the one shared truth; a change publishes the key as a topic.
- `settings`: schema-versioned, migrations, atomic whole-map save, read-only when stored by newer firmware.
- `system_settings`: the product schema (v1). Every change: bump version + migration + test (hard rule 4).
- `panel_api` / `registry`: manifest, lifecycle `create → show → tick → hide → destroy`, `PanelContext` (state, own settings, declared topics only).
