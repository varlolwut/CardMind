#pragma once

#include "app_types.h"

namespace cardputer {

enum class SshAuthMode {
    Password,
    PrivateKey,
};

struct SshProfile {
    String name;
    String host;
    std::uint16_t port;
    String username;
    String password;
    SshAuthMode authMode;
    String privateKeyPassphrase;
};

constexpr std::size_t kMaximumSshProfiles = 8;

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

struct SftpEntry {
    String name;
    bool directory;
    std::uint64_t size;
};

struct SftpEntriesResult {
    bool success;
    std::vector<SftpEntry> entries;
    String error;
};

OperationResult loadSshProfile(SshProfile& profile);
OperationResult saveSshProfile(const SshProfile& profile);
OperationResult loadSshProfiles(std::vector<SshProfile>& profiles,
                                std::size_t& selectedIndex);
OperationResult saveSshProfileAt(const SshProfile& profile,
                                 std::size_t index);
OperationResult selectSshProfile(std::size_t index);
OperationResult deleteSshProfile(std::size_t index);
bool sshProfileIsComplete(const SshProfile& profile);
OperationResult initializeSshStorage();
OperationResult installSshPrivateKey(const String& temporaryPath);
bool sshPrivateKeyIsInstalled();
SshTrustResult checkTrustedSshHost(const String& host, std::uint16_t port,
                                   const String& fingerprint);
OperationResult trustSshHost(const String& host, std::uint16_t port,
                             const String& fingerprint);
OperationResult forgetTrustedSshHost(const String& host, std::uint16_t port);

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
    OperationResult executeCommand(const String& command,
                                   std::string& output,
                                   int& exitStatus,
                                   std::size_t maximumOutputBytes,
                                   std::uint32_t timeoutMs);
    OperationResult openSftp(std::uint32_t timeoutMs);
    SftpEntriesResult listSftpDirectory(const String& path,
                                        std::uint32_t timeoutMs);
    OperationResult downloadSftpFile(const String& remotePath,
                                     const String& workspaceName,
                                     std::uint32_t timeoutMs);
    OperationResult uploadSftpFile(const String& workspaceName,
                                   const String& remotePath,
                                   std::uint32_t timeoutMs);
    OperationResult createSftpDirectory(const String& path,
                                        std::uint32_t timeoutMs);
    OperationResult removeSftpPath(const String& path, bool directory,
                                   std::uint32_t timeoutMs);
    OperationResult renameSftpPath(const String& sourcePath,
                                   const String& destinationPath,
                                   std::uint32_t timeoutMs);
    bool isOpen() const;
    bool isSftpOpen() const;
    String fingerprint() const;
    String hostKeyType() const;
    void close();

private:
    struct Implementation;
    Implementation* implementation_;
};

}  // namespace cardputer
