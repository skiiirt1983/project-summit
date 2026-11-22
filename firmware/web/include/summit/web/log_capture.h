// Device only: copies everything the firmware logs (ESP_LOGx) into the web UI's log buffer, and still
// prints it on the serial console as before.
#pragma once
#include "summit/web/log_buffer.h"

namespace summit::web {
LogBuffer &device_log();   // the one buffer the web UI serves
void log_capture_start();  // call first thing in app_main
}  // namespace summit::web
