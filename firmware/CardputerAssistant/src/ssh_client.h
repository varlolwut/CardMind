#pragma once

#include "app_types.h"

namespace cardputer {

enum class SshAuthMode {
    Password,
    PrivateKey,
};

struct SshProfile {
    String host;
    std::uint16_t port;
    String username;
    String password;
    SshAuthMode authMode;
    String privateKeyPassphrase;
};

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

struct SshTrustResult {
    bool success;
    bool found;
    bool matches;
    String trustedFingerprint;
    String error;
};

OperationResult loadSshProfile(SshProfile& profile);
OperationResult saveSshProfile(const SshProfile& profile);
bool sshProfileIsComplete(const SshProfile& profile);
OperationResult initializeSshStorage();
OperationResult installSshPrivateKey(const String& temporaryPath);
bool sshPrivateKeyIsInstalled();
SshTrustResult checkTrustedSshHost(const String& host, std::uint16_t port,
                                   const String& fingerprint);
OperationResult trustSshHost(const String& host, std::uint16_t port,
                             const String& fingerprint);

SshHostProbeResult probeSshHost(const String& host, std::uint16_t port,
                                std::uint32_t timeoutMs);

class SshClient {
public:
    SshClient();
    ~SshClient();

    OperationResult connect(const SshProfile& profile, std::uint32_t timeoutMs);
    OperationResult authenticate(const SshProfile& profile,
                                 std::uint32_t timeoutMs);
    OperationResult openTerminal(std::uint32_t columns, std::uint32_t rows,
                                 std::uint32_t timeoutMs);
    int read(std::uint8_t* output, std::size_t maximumBytes);
    OperationResult write(const std::uint8_t* data, std::size_t bytes,
                          std::uint32_t timeoutMs);
    bool isOpen() const;
    String fingerprint() const;
    String hostKeyType() const;
    void close();

private:
    struct Implementation;
    Implementation* implementation_;
};

}  // namespace cardputer
