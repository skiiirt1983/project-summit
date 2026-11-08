#include "summit/update/preflight.h"

namespace summit::update {

uint32_t preflight(const PreflightInput &in)
{
    uint32_t f = 0;
    if (!in.on_usb_power && in.battery_pct < kMinBatteryPct) f |= kNeedPower;
    if (in.next_alarm_in_s >= 0 && in.next_alarm_in_s < kAlarmGuardS) f |= kAlarmSoon;
    if (!in.wifi_connected) f |= kNoWifi;
    if (in.from_network && !in.time_synced) f |= kNoTime;
    if (in.image_size <= 0 || in.image_size > in.slot_size) f |= kDoesNotFit;
    return f;
}

std::string preflight_message(uint32_t f)
{
    if (f & kDoesNotFit) return "Update image does not fit the update slot";
    if (f & kNeedPower) return "Connect USB power or charge to 40% first";
    if (f & kAlarmSoon) return "An alarm is due within 15 minutes";
    if (f & kNoWifi) return "Wi-Fi is not connected";
    if (f & kNoTime) return "Waiting for the clock to sync";
    return "";
}

}  // namespace summit::update
