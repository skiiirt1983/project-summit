// Wi-Fi service (device only). Connects to the saved network, scans, and publishes the state the web UI and the
// glance strip read:  net/wifi_connected, net/ssid, net/ip, net/scan  (ARCHITECTURE.md "State topics").
// The web UI saves credentials in NVS namespace "wifi" (keys ssid, pass) and publishes net/connect.
// With no network saved, or none reachable for about 45 s, it also runs a setup access point
// ("Summit-Setup", WPA2, password "summit123": Paul's choice for now) so the web UI can be reached at
// http://192.168.4.1 to enter the home network. It is isolated: nothing is forwarded to the home network.
#pragma once
#include "summit/event_bus.h"
#include "summit/state_store.h"

namespace summit::net {
// Starts the network stack and a background task; returns at once (Wi-Fi comes up a few seconds later).
// Call before the web server so the default event loop exists.
bool wifi_service_start(EventBus &bus, StateStore &state);
}  // namespace summit::net
