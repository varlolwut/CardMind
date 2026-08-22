#include "wifi_networks.h"

#include <WiFi.h>

namespace cardputer {

WifiScanResult scanWifiNetworks()
{
    const int count = WiFi.scanNetworks(false, true);
    if (count < 0) {
        return {false, {}, "Wi-Fi scan failed with code " + String(count)};
    }
    std::vector<WifiNetwork> networks;
    networks.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) {
            continue;
        }
        bool duplicate = false;
        for (const auto& network : networks) {
            if (network.ssid == ssid) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            networks.push_back({ssid, WiFi.RSSI(index), WiFi.encryptionType(index) != WIFI_AUTH_OPEN});
        }
    }
    WiFi.scanDelete();
    return {true, networks, ""};
}

}  // namespace cardputer
