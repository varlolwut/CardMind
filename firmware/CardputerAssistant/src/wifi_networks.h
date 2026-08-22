#pragma once

#include "app_types.h"

#include <cstdint>
#include <vector>

namespace cardputer {

struct WifiNetwork {
    String ssid;
    std::int32_t rssi;
    bool secured;
};

struct WifiScanResult {
    bool success;
    std::vector<WifiNetwork> networks;
    String error;
};

WifiScanResult scanWifiNetworks();

}  // namespace cardputer
