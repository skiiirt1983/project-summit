# firmware/framework

The app framework, the seam between panels and everything else (ARCHITECTURE.md "Panel contract").
Plain C++17, no ESP-IDF or LVGL dependency, so `tests/run_tests.sh` builds and tests the same sources on the host.

- `event_bus`: any task publishes; the UI task dispatches. Bounded queue, drops oldest when full (counted).
- `state_store`: the one shared truth; a change publishes the key as a topic.
- `settings`: schema-versioned with migrations; atomic whole-blob save; unknown keys preserved. Newer/unmigratable/corrupt settings raise a notice and are never overwritten until an explicit reset, which backs up the old blob first.
- `system_settings`: the product schema (v1). Adding a key needs no bump; rename/remove/type or meaning change needs a bump + migration + test (hard rule 4).
- `panel_api` / `registry`: manifest, lifecycle `create → show → tick → hide → destroy`, `PanelContext` (state, own settings, declared topics only).
