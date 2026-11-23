// Device only: copies everything the firmware logs (ESP_LOGx) into the web UI's log buffer, and still
// prints it on the serial console as before.
#pragma once
#include "summit/storage.h"
#include "summit/web/log_buffer.h"

namespace summit::web {
LogBuffer &device_log();   // the one buffer the web UI serves
void log_capture_start();  // call first thing in app_main
// Keeps the log on the SD card in rolling files (log_mirror.h) once a card is mounted, and shows the previous boot's log at
// the top of the Log tab when a power cut or reset lost the copy in RAM. Call after the SD card service has started.
void log_mirror_start(Storage &storage);
}  // namespace summit::web
