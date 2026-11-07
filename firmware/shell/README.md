# firmware/shell

Glance strip, pager (one panel per page), quick-settings sheet, toasts and rotation re-layout.
Pure logic in `src/{layout,glance,strip_state,rotation,pager}.cpp` (host tests in `tests/host/test_shell_logic.cpp`);
LVGL front end in `src/shell_ui.cpp`, run in the PC simulator (`firmware/sim`) and on the device.
