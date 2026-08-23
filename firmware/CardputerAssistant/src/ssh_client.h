#pragma once

#include "app_types.h"

namespace cardputer {

struct SshRuntimeProbeResult {
    bool success;
    String version;
    String error;
};

SshRuntimeProbeResult probeSshRuntime();

struct SshHostProbeResult {
    bool success;
    String fingerprint;
    String hostKeyType;
    String error;
};

SshHostProbeResult probeSshHost(const String& host, std::uint16_t port,
                                std::uint32_t timeoutMs);

}  // namespace cardputer
