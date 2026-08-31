#pragma once

#include "app_types.h"

#include <functional>

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
    std::uint64_t privateKeyId = 0;
};

struct SshProfileSummary {
    std::uint64_t id;
    String name;
    String host;
    std::uint16_t port;
    String username;
    SshAuthMode authMode;
};

struct SshAuthoritySummary {
    std::uint64_t profileId;
    String name;
    String host;
    std::uint16_t port;
    String username;
    SshAuthMode authMode;
    std::uint64_t privateKeyId;
};

constexpr std::size_t kMaximumSshProfiles = 5;

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

struct SftpPageResult {
    bool success;
    std::vector<SftpEntry> entries;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct SftpReadResult {
    bool success;
    std::string content;
    std::uint64_t nextOffset;
    std::uint64_t totalBytes;
    bool eof;
    String error;
};

struct SftpMutationResult {
    bool success;
    bool outcomeUnknown;
    String error;
};

using SshCommandOutputCallback =
    std::function<OperationResult(const std::uint8_t*, std::size_t)>;

OperationResult loadSshProfile(SshProfile& profile);
OperationResult loadSshProfileWithId(SshProfile& profile,
                                     std::uint64_t& profileId);
OperationResult loadSelectedSshAuthority(SshAuthoritySummary& authority);
OperationResult saveSshProfile(const SshProfile& profile);
OperationResult loadSshProfileSummaries(
    std::vector<SshProfileSummary>& profiles,
    std::size_t& selectedIndex);
OperationResult loadSshProfiles(std::vector<SshProfile>& profiles,
                                std::size_t& selectedIndex);
OperationResult saveSshProfileAt(const SshProfile& profile,
                                 std::size_t index);
OperationResult selectSshProfile(std::size_t index);
OperationResult deleteSshProfile(std::size_t index);
bool sshProfileIsComplete(const SshProfile& profile);
OperationResult initializeSshStorage();
OperationResult installSshPrivateKey(const String& temporaryPath,
                                     std::uint64_t profileId);
bool sshPrivateKeyIsInstalled(std::uint64_t privateKeyId);
OperationResult loadTrustedSshFingerprint(const String& host,
                                          std::uint16_t port,
                                          String& fingerprint,
                                          bool& found);
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
    OperationResult connectControlled(
        const SshProfile& profile,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    OperationResult authenticate(const SshProfile& profile,
                                 std::uint32_t timeoutMs);
    OperationResult authenticateControlled(
        const SshProfile& profile,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    OperationResult openTerminal(std::uint32_t columns, std::uint32_t rows,
                                 std::uint32_t timeoutMs);
    OperationResult resizeTerminal(std::uint32_t columns, std::uint32_t rows,
                                   std::uint32_t timeoutMs);
    int read(std::uint8_t* output, std::size_t maximumBytes);
    OperationResult write(const std::uint8_t* data, std::size_t bytes,
                          std::uint32_t timeoutMs);
    OperationResult executeCommand(const String& command,
                                   std::string& output,
                                   int& exitStatus,
                                   std::size_t maximumOutputBytes,
                                   std::uint32_t timeoutMs);
    OperationResult executeCommandControlled(
        const String& command,
        std::string& output,
        int& exitStatus,
        std::size_t maximumOutputBytes,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    OperationResult executeCommandStreamingControlled(
        const String& command,
        int& exitStatus,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled,
        const SshCommandOutputCallback& onOutput);
    OperationResult openSftp(std::uint32_t timeoutMs);
    OperationResult openSftpControlled(
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpEntriesResult listSftpDirectory(const String& path,
                                        std::uint32_t timeoutMs);
    SftpPageResult listSftpDirectoryPageControlled(
        const String& path,
        std::uint32_t offset,
        std::size_t maximumEntries,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpReadResult readSftpFileChunkControlled(
        const String& path,
        std::uint64_t offset,
        std::size_t maximumBytes,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpMutationResult writeSftpTextFileControlled(
        const String& path,
        const std::string& content,
        bool overwrite,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpMutationResult moveSftpPathControlled(
        const String& sourcePath,
        const String& destinationPath,
        bool overwrite,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpMutationResult downloadSftpFileControlled(
        const String& remotePath,
        const String& workspaceName,
        bool overwrite,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
    SftpMutationResult uploadSftpFileControlled(
        const String& workspaceName,
        const String& remotePath,
        bool overwrite,
        std::uint32_t timeoutMs,
        const std::function<bool()>& isCancelled);
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
