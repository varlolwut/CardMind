#include "ssh_client.h"

#include "file_workspace.h"

#include <libssh2.h>
#include <libssh2_sftp.h>
#define public public_key
#include <libssh2_priv.h>
#undef public

#include <NetworkClient.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <cctype>
#include <new>
#include <sys/socket.h>
#include <vector>

namespace cardputer {
namespace {

constexpr const char* kSshNamespace = "cardmind_ssh";
constexpr const char* kSshDirectory = "/assistant/ssh";
constexpr const char* kSshPrivateKeyPath = "/assistant/ssh/id.pem";
constexpr const char* kSshPrivateKeyVfsPath = "/sd/assistant/ssh/id.pem";
constexpr const char* kSshKnownHostsPath = "/assistant/ssh/known_hosts";
constexpr const char* kSshKnownHostsTemporaryPath = "/assistant/ssh/known_hosts.tmp";
constexpr std::size_t kMaximumKnownHostsBytes = 16384;
constexpr unsigned int kTerminalWindowBytes = 64 * 1024;
constexpr unsigned int kTerminalPacketBytes = 8 * 1024;

struct SshAllocatorState {
    std::size_t failedAllocationBytes;
    std::uint32_t capabilities;
};

void* allocateSshMemory(std::size_t count, void** abstract)
{
    SshAllocatorState* state = abstract != nullptr
        ? static_cast<SshAllocatorState*>(*abstract) : nullptr;
    void* memory = state == nullptr ? nullptr : heap_caps_malloc(count, state->capabilities);
    if (memory == nullptr && abstract != nullptr && *abstract != nullptr) {
        state->failedAllocationBytes = count;
    }
    return memory;
}

void releaseSshMemory(void* pointer, void** abstract)
{
    static_cast<void>(abstract);
    heap_caps_free(pointer);
}

void* reallocateSshMemory(void* pointer, std::size_t count, void** abstract)
{
    SshAllocatorState* state = abstract != nullptr
        ? static_cast<SshAllocatorState*>(*abstract) : nullptr;
    void* memory = state == nullptr
        ? nullptr : heap_caps_realloc(pointer, count, state->capabilities);
    if (memory == nullptr && abstract != nullptr && *abstract != nullptr) {
        state->failedAllocationBytes = count;
    }
    return memory;
}

bool isValidSshHost(const String& host)
{
    if (host.isEmpty() || host.length() > 253 || host.indexOf(' ') >= 0 ||
        host.indexOf('\t') >= 0 || host.indexOf('\r') >= 0 || host.indexOf('\n') >= 0) {
        return false;
    }
    return true;
}

bool isValidSshUsername(const String& username)
{
    return !username.isEmpty() && username.length() <= 64 &&
           username.indexOf('\r') < 0 && username.indexOf('\n') < 0;
}

bool isValidSshProfileName(const String& name)
{
    return !name.isEmpty() && name.length() <= 32 &&
           name.indexOf('\r') < 0 && name.indexOf('\n') < 0;
}

OperationResult validateSshProfile(const SshProfile& profile)
{
    if (!isValidSshProfileName(profile.name)) {
        return {false, "SSH profile name must contain 1 to 32 characters"};
    }
    if (!isValidSshHost(profile.host)) {
        return {false, "SSH host must contain 1 to 253 characters without whitespace"};
    }
    if (profile.port == 0) {
        return {false, "SSH port must be between 1 and 65535"};
    }
    if (!isValidSshUsername(profile.username)) {
        return {false, "SSH username must contain 1 to 64 characters"};
    }
    if (profile.password.length() > 192 || profile.privateKeyPassphrase.length() > 192) {
        return {false, "SSH password and key passphrase must not exceed 192 characters"};
    }
    if (profile.authMode == SshAuthMode::Password && profile.password.isEmpty()) {
        return {false, "SSH password authentication requires a password"};
    }
    if (profile.authMode == SshAuthMode::PrivateKey && !sshPrivateKeyIsInstalled()) {
        return {false, "SSH private-key authentication requires an installed key"};
    }
    return {true, ""};
}

SshProfile readIndexedProfile(Preferences& preferences, std::size_t index)
{
    const String suffix(index);
    return {
        preferences.getString((String("n") + suffix).c_str(), ""),
        preferences.getString((String("h") + suffix).c_str(), ""),
        preferences.getUShort((String("p") + suffix).c_str(), 22),
        preferences.getString((String("u") + suffix).c_str(), ""),
        preferences.getString((String("w") + suffix).c_str(), ""),
        preferences.getUChar((String("a") + suffix).c_str(), 0) == 1
            ? SshAuthMode::PrivateKey : SshAuthMode::Password,
        preferences.getString((String("k") + suffix).c_str(), ""),
    };
}

bool writeIndexedProfile(Preferences& preferences, const SshProfile& profile,
                         std::size_t index)
{
    const String suffix(index);
    return preferences.putString((String("n") + suffix).c_str(), profile.name) == profile.name.length() &&
           preferences.putString((String("h") + suffix).c_str(), profile.host) == profile.host.length() &&
           preferences.putUShort((String("p") + suffix).c_str(), profile.port) == sizeof(std::uint16_t) &&
           preferences.putString((String("u") + suffix).c_str(), profile.username) == profile.username.length() &&
           preferences.putString((String("w") + suffix).c_str(), profile.password) == profile.password.length() &&
           preferences.putUChar((String("a") + suffix).c_str(),
                                profile.authMode == SshAuthMode::PrivateKey ? 1 : 0) == sizeof(std::uint8_t) &&
           preferences.putString((String("k") + suffix).c_str(), profile.privateKeyPassphrase) ==
               profile.privateKeyPassphrase.length();
}

void removeIndexedProfile(Preferences& preferences, std::size_t index)
{
    const String suffix(index);
    preferences.remove((String("n") + suffix).c_str());
    preferences.remove((String("h") + suffix).c_str());
    preferences.remove((String("p") + suffix).c_str());
    preferences.remove((String("u") + suffix).c_str());
    preferences.remove((String("w") + suffix).c_str());
    preferences.remove((String("a") + suffix).c_str());
    preferences.remove((String("k") + suffix).c_str());
}

OperationResult writeProfiles(const std::vector<SshProfile>& profiles,
                              std::size_t selectedIndex)
{
    if (profiles.size() > kMaximumSshProfiles ||
        (!profiles.empty() && selectedIndex >= profiles.size())) {
        return {false, "SSH profile collection is invalid"};
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH settings in NVS for writing"};
    }
    bool written = true;
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        written = writeIndexedProfile(preferences, profiles[index], index) && written;
    }
    for (std::size_t index = profiles.size(); index < kMaximumSshProfiles; ++index) {
        removeIndexedProfile(preferences, index);
    }
    written = preferences.putUChar("cnt", static_cast<std::uint8_t>(profiles.size())) ==
                  sizeof(std::uint8_t) && written;
    written = preferences.putUChar("sel", profiles.empty() ? 0 :
                  static_cast<std::uint8_t>(selectedIndex)) == sizeof(std::uint8_t) && written;
    preferences.remove("host");
    preferences.remove("port");
    preferences.remove("user");
    preferences.remove("password");
    preferences.remove("auth_mode");
    preferences.remove("key_pass");
    preferences.end();
    if (!written) {
        return {false, "Failed to write the complete SSH profile collection to NVS"};
    }
    std::vector<SshProfile> verified;
    std::size_t verifiedSelected = 0;
    const OperationResult loaded = loadSshProfiles(verified, verifiedSelected);
    if (!loaded.success || verified.size() != profiles.size() ||
        (!profiles.empty() && verifiedSelected != selectedIndex)) {
        return {false, "Failed to verify SSH profiles after NVS write"};
    }
    return {true, ""};
}

bool isValidSshFingerprint(const String& fingerprint)
{
    if (fingerprint.length() != 95) {
        return false;
    }
    for (std::size_t index = 0; index < fingerprint.length(); ++index) {
        if (index % 3 == 2) {
            if (fingerprint[index] != ':') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(fingerprint[index]))) {
            return false;
        }
    }
    return true;
}

String knownHostPrefix(const String& host, std::uint16_t port)
{
    return host + "\t" + String(port) + "\t";
}

bool isValidRemotePath(const String& path)
{
    return !path.isEmpty() && path.length() <= 511 && path[0] == '/' &&
           path.indexOf('\r') < 0 && path.indexOf('\n') < 0;
}

OperationResult ensureSshDirectory()
{
    if (SD.exists(kSshDirectory)) {
        return {true, ""};
    }
    if (!SD.mkdir(kSshDirectory)) {
        return {false, "Failed to create SSH storage directory on microSD"};
    }
    return {true, ""};
}

String sshHostKeyTypeName(int hostKeyType)
{
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_RSA) {
        return "RSA";
    }
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_DSS) {
        return "DSA";
    }
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_256) {
        return "ECDSA-256";
    }
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_384) {
        return "ECDSA-384";
    }
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_521) {
        return "ECDSA-521";
    }
    if (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ED25519) {
        return "ED25519";
    }
    return "unknown";
}

OperationResult formatSessionHostKey(LIBSSH2_SESSION* session, String& fingerprint,
                                     String& hostKeyType)
{
    size_t hostKeyLength = 0;
    int hostKeyTypeValue = 0;
    const char* hostKey = libssh2_session_hostkey(
        session, &hostKeyLength, &hostKeyTypeValue);
    const unsigned char* hash = reinterpret_cast<const unsigned char*>(
        libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256));
    if (hostKey == nullptr || hostKeyLength == 0 || hash == nullptr) {
        return {false, "SSH server did not provide a usable SHA-256 host key"};
    }
    char formatted[96] = {};
    std::size_t position = 0;
    for (std::size_t index = 0; index < 32; ++index) {
        const int written = std::snprintf(formatted + position,
                                          sizeof(formatted) - position,
                                          index == 0 ? "%02X" : ":%02X", hash[index]);
        if (written <= 0 || static_cast<std::size_t>(written) >=
                            sizeof(formatted) - position) {
            return {false, "SSH SHA-256 fingerprint formatting failed"};
        }
        position += static_cast<std::size_t>(written);
    }
    fingerprint = formatted;
    hostKeyType = sshHostKeyTypeName(hostKeyTypeValue);
    return {true, ""};
}

String sessionError(LIBSSH2_SESSION* session, const String& action, int errorCode)
{
    char* detail = nullptr;
    int detailLength = 0;
    libssh2_session_last_error(session, &detail, &detailLength, 0);
    const String reason = detail != nullptr && detailLength > 0
        ? String(detail).substring(0, static_cast<unsigned int>(detailLength))
        : String("no protocol detail");
    return action + " failed with code " + String(errorCode) + ": " + reason;
}

template <typename Attempt>
int runUntilComplete(Attempt attempt, std::uint32_t timeoutMs)
{
    const std::uint32_t deadline = millis() + timeoutMs;
    int result = LIBSSH2_ERROR_EAGAIN;
    while (result == LIBSSH2_ERROR_EAGAIN &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        result = attempt();
        delay(5);
    }
    return result == LIBSSH2_ERROR_EAGAIN ? LIBSSH2_ERROR_TIMEOUT : result;
}

void closeSftpHandle(LIBSSH2_SFTP_HANDLE* handle)
{
    if (handle == nullptr) return;
    runUntilComplete([handle]() { return libssh2_sftp_close_handle(handle); }, 5000);
}

}  // namespace

OperationResult loadSshProfile(SshProfile& profile)
{
    std::vector<SshProfile> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult result = loadSshProfiles(profiles, selectedIndex);
    if (!result.success) {
        return result;
    }
    if (profiles.empty()) {
        profile = {"", "", 22, "", "", SshAuthMode::Password, ""};
        return {true, ""};
    }
    profile = profiles[selectedIndex];
    return {true, ""};
}

OperationResult loadSshProfiles(std::vector<SshProfile>& profiles,
                                std::size_t& selectedIndex)
{
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH settings in NVS"};
    }
    const std::size_t count = preferences.getUChar("cnt", 0);
    if (count > kMaximumSshProfiles) {
        preferences.end();
        return {false, "Stored SSH profile count exceeds the supported limit"};
    }
    std::vector<SshProfile> loaded;
    loaded.reserve(count == 0 ? 1 : count);
    for (std::size_t index = 0; index < count; ++index) {
        SshProfile item = readIndexedProfile(preferences, index);
        if (!isValidSshProfileName(item.name) || !isValidSshHost(item.host) ||
            item.port == 0 || !isValidSshUsername(item.username)) {
            preferences.end();
            return {false, "Stored SSH profile " + String(index + 1) + " is invalid"};
        }
        loaded.push_back(std::move(item));
    }
    std::size_t selected = count == 0 ? 0 : preferences.getUChar("sel", 0);
    if (count == 0) {
        const SshProfile legacy = {
        preferences.getString("host", ""),
        preferences.getString("host", ""),
        preferences.getUShort("port", 22),
        preferences.getString("user", ""),
        preferences.getString("password", ""),
        preferences.getUChar("auth_mode", 0) == 1
            ? SshAuthMode::PrivateKey : SshAuthMode::Password,
        preferences.getString("key_pass", ""),
        };
        if (!legacy.host.isEmpty()) {
            SshProfile migrated = legacy;
            migrated.name = legacy.host.substring(0, 32);
            loaded.push_back(std::move(migrated));
        }
    } else if (selected >= count) {
        preferences.end();
        return {false, "Stored selected SSH profile index is invalid"};
    }
    preferences.end();
    profiles = std::move(loaded);
    selectedIndex = selected;
    return {true, ""};
}

OperationResult saveSshProfile(const SshProfile& profile)
{
    std::vector<SshProfile> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfiles(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    SshProfile named = profile;
    if (named.name.isEmpty()) {
        named.name = named.host.substring(0, 32);
    }
    return saveSshProfileAt(named, profiles.empty() ? 0 : selectedIndex);
}

OperationResult saveSshProfileAt(const SshProfile& profile, std::size_t index)
{
    const OperationResult valid = validateSshProfile(profile);
    if (!valid.success) {
        return valid;
    }
    std::vector<SshProfile> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfiles(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    if (index > profiles.size() || index >= kMaximumSshProfiles) {
        return {false, "SSH profile index is outside the supported range"};
    }
    if (index == profiles.size()) {
        profiles.push_back(profile);
        selectedIndex = index;
    } else {
        profiles[index] = profile;
    }
    return writeProfiles(profiles, selectedIndex);
}

OperationResult selectSshProfile(std::size_t index)
{
    std::vector<SshProfile> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfiles(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    if (index >= profiles.size()) {
        return {false, "SSH profile selection is outside the stored range"};
    }
    return writeProfiles(profiles, index);
}

OperationResult deleteSshProfile(std::size_t index)
{
    std::vector<SshProfile> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfiles(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    if (index >= profiles.size()) {
        return {false, "SSH profile deletion is outside the stored range"};
    }
    profiles.erase(profiles.begin() + static_cast<std::ptrdiff_t>(index));
    if (profiles.empty()) {
        selectedIndex = 0;
    } else if (selectedIndex > index) {
        --selectedIndex;
    } else if (selectedIndex >= profiles.size()) {
        selectedIndex = profiles.size() - 1;
    }
    return writeProfiles(profiles, selectedIndex);
}

bool sshProfileIsComplete(const SshProfile& profile)
{
    return isValidSshHost(profile.host) && profile.port != 0 &&
           isValidSshUsername(profile.username) &&
           ((profile.authMode == SshAuthMode::Password && !profile.password.isEmpty()) ||
            (profile.authMode == SshAuthMode::PrivateKey && sshPrivateKeyIsInstalled()));
}

OperationResult initializeSshStorage()
{
    return ensureSshDirectory();
}

OperationResult installSshPrivateKey(const String& temporaryPath)
{
    const OperationResult directory = ensureSshDirectory();
    if (!directory.success) {
        return directory;
    }
    File source = SD.open(temporaryPath, FILE_READ);
    if (!source || source.isDirectory()) {
        if (source) {
            source.close();
        }
        return {false, "SSH private-key upload could not be opened"};
    }
    const std::size_t size = source.size();
    if (size < 64 || size > 16384) {
        source.close();
        return {false, "SSH private key must contain 64 to 16384 bytes"};
    }
    char header[65] = {};
    const std::size_t headerBytes = source.readBytes(header, sizeof(header) - 1);
    source.seek(0);
    if (headerBytes < 16 || String(header).indexOf("-----BEGIN ") < 0) {
        source.close();
        return {false, "SSH private key is not a PEM document"};
    }
    const String replacementPath = String(kSshPrivateKeyPath) + ".new";
    SD.remove(replacementPath);
    File output = SD.open(replacementPath, FILE_WRITE);
    if (!output) {
        source.close();
        return {false, "Failed to create temporary SSH private-key file on microSD"};
    }
    std::uint8_t buffer[512] = {};
    std::size_t copied = 0;
    while (source.available()) {
        const std::size_t readBytes = source.read(buffer, sizeof(buffer));
        if (readBytes == 0 || output.write(buffer, readBytes) != readBytes) {
            source.close();
            output.close();
            SD.remove(replacementPath);
            return {false, "Failed while writing SSH private key to microSD"};
        }
        copied += readBytes;
    }
    source.close();
    output.flush();
    output.close();
    if (copied != size) {
        SD.remove(replacementPath);
        return {false, "SSH private-key copy length did not match the uploaded file"};
    }
    if (SD.exists(kSshPrivateKeyPath) && !SD.remove(kSshPrivateKeyPath)) {
        SD.remove(replacementPath);
        return {false, "Failed to replace the existing SSH private key"};
    }
    if (!SD.rename(replacementPath, kSshPrivateKeyPath)) {
        SD.remove(replacementPath);
        return {false, "Failed to activate the SSH private key on microSD"};
    }
    return {true, ""};
}

bool sshPrivateKeyIsInstalled()
{
    File key = SD.open(kSshPrivateKeyPath, FILE_READ);
    if (!key || key.isDirectory()) {
        if (key) {
            key.close();
        }
        return false;
    }
    const bool validSize = key.size() >= 64 && key.size() <= 16384;
    key.close();
    return validSize;
}

SshTrustResult checkTrustedSshHost(const String& host, std::uint16_t port,
                                   const String& fingerprint)
{
    if (!isValidSshHost(host) || port == 0 || !isValidSshFingerprint(fingerprint)) {
        return {false, false, false, "", "SSH trust lookup received invalid host data"};
    }
    File file = SD.open(kSshKnownHostsPath, FILE_READ);
    if (!file) {
        return {true, false, false, "", ""};
    }
    if (file.isDirectory() || file.size() > kMaximumKnownHostsBytes) {
        file.close();
        return {false, false, false, "", "SSH known-hosts file is invalid or too large"};
    }
    const String prefix = knownHostPrefix(host, port);
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith(prefix)) {
            const String trusted = line.substring(prefix.length());
            file.close();
            if (!isValidSshFingerprint(trusted)) {
                return {false, true, false, "", "Stored SSH host fingerprint is invalid"};
            }
            return {true, true, trusted == fingerprint, trusted, ""};
        }
    }
    file.close();
    return {true, false, false, "", ""};
}

OperationResult trustSshHost(const String& host, std::uint16_t port,
                             const String& fingerprint)
{
    if (!isValidSshHost(host) || port == 0 || !isValidSshFingerprint(fingerprint)) {
        return {false, "Cannot trust an invalid SSH host or fingerprint"};
    }
    const OperationResult directory = ensureSshDirectory();
    if (!directory.success) {
        return directory;
    }
    File source = SD.open(kSshKnownHostsPath, FILE_READ);
    if (source && (source.isDirectory() || source.size() > kMaximumKnownHostsBytes)) {
        source.close();
        return {false, "SSH known-hosts file is invalid or too large"};
    }
    SD.remove(kSshKnownHostsTemporaryPath);
    File output = SD.open(kSshKnownHostsTemporaryPath, FILE_WRITE);
    if (!output) {
        if (source) {
            source.close();
        }
        return {false, "Failed to create temporary SSH known-hosts file"};
    }
    const String prefix = knownHostPrefix(host, port);
    bool replaced = false;
    while (source && source.available()) {
        String line = source.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(prefix)) {
            line = prefix + fingerprint;
            replaced = true;
        }
        if (output.println(line) == 0) {
            source.close();
            output.close();
            SD.remove(kSshKnownHostsTemporaryPath);
            return {false, "Failed while updating SSH known-hosts file"};
        }
    }
    if (source) {
        source.close();
    }
    if (!replaced && output.println(prefix + fingerprint) == 0) {
        output.close();
        SD.remove(kSshKnownHostsTemporaryPath);
        return {false, "Failed while appending SSH trusted host"};
    }
    output.flush();
    output.close();
    if (SD.exists(kSshKnownHostsPath) && !SD.remove(kSshKnownHostsPath)) {
        SD.remove(kSshKnownHostsTemporaryPath);
        return {false, "Failed to replace SSH known-hosts file"};
    }
    if (!SD.rename(kSshKnownHostsTemporaryPath, kSshKnownHostsPath)) {
        SD.remove(kSshKnownHostsTemporaryPath);
        return {false, "Failed to activate SSH known-hosts file"};
    }
    return {true, ""};
}

OperationResult forgetTrustedSshHost(const String& host, std::uint16_t port)
{
    if (!isValidSshHost(host) || port == 0) {
        return {false, "Cannot forget an invalid SSH host"};
    }
    File source = SD.open(kSshKnownHostsPath, FILE_READ);
    if (!source) {
        return {true, ""};
    }
    if (source.isDirectory() || source.size() > kMaximumKnownHostsBytes) {
        source.close();
        return {false, "SSH known-hosts file is invalid or too large"};
    }
    SD.remove(kSshKnownHostsTemporaryPath);
    File output = SD.open(kSshKnownHostsTemporaryPath, FILE_WRITE);
    if (!output) {
        source.close();
        return {false, "Failed to create temporary SSH known-hosts file"};
    }
    const String prefix = knownHostPrefix(host, port);
    while (source.available()) {
        String line = source.readStringUntil('\n');
        line.trim();
        if (!line.isEmpty() && !line.startsWith(prefix) && output.println(line) == 0) {
            source.close();
            output.close();
            SD.remove(kSshKnownHostsTemporaryPath);
            return {false, "Failed while removing an SSH trusted host"};
        }
    }
    source.close();
    output.flush();
    output.close();
    if (!SD.remove(kSshKnownHostsPath) ||
        !SD.rename(kSshKnownHostsTemporaryPath, kSshKnownHostsPath)) {
        SD.remove(kSshKnownHostsTemporaryPath);
        return {false, "Failed to activate the updated SSH known-hosts file"};
    }
    return {true, ""};
}

SshRuntimeProbeResult probeSshRuntime()
{
    const int initialization = libssh2_init(0);
    if (initialization != 0) {
        return {false, "", "libssh2 initialization failed with code " + String(initialization)};
    }
    const char* version = libssh2_version(LIBSSH2_VERSION_NUM);
    if (version == nullptr) {
        libssh2_exit();
        return {false, "", "libssh2 did not report a compatible runtime version"};
    }
    const String resultVersion(version);
    libssh2_exit();
    return {true, resultVersion, ""};
}

SshHostProbeResult probeSshHost(const String& host, std::uint16_t port,
                                std::uint32_t timeoutMs)
{
    if (host.isEmpty() || host.length() > 253 || host.indexOf(' ') >= 0) {
        return {false, "", "", "SSH host must contain 1 to 253 characters without spaces"};
    }
    if (port == 0) {
        return {false, "", "", "SSH port must be between 1 and 65535"};
    }
    if (timeoutMs < 1000 || timeoutMs > 60000) {
        return {false, "", "", "SSH timeout must be between 1000 and 60000 ms"};
    }
    const int initialization = libssh2_init(0);
    if (initialization != 0) {
        return {false, "", "", "libssh2 initialization failed with code " + String(initialization)};
    }

    const std::size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const std::uint32_t allocationCapabilities = freePsram > 0
        ? static_cast<std::uint32_t>(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
        : static_cast<std::uint32_t>(MALLOC_CAP_8BIT);
    const std::size_t freeAllocatorMemory = heap_caps_get_free_size(allocationCapabilities);

    NetworkClient client;
    client.setTimeout(timeoutMs);
    if (!client.connect(host.c_str(), port, static_cast<int32_t>(timeoutMs))) {
        libssh2_exit();
        return {false, "", "", "TCP connection to " + host + ":" + String(port) + " failed"};
    }
    const std::uint32_t bannerDeadline = millis() + std::min<std::uint32_t>(timeoutMs, 5000);
    char bannerFirstByte = 0;
    int bannerPeek = -1;
    while (bannerPeek <= 0 && static_cast<std::int32_t>(bannerDeadline - millis()) > 0) {
        bannerPeek = recv(client.fd(), &bannerFirstByte, 1, MSG_PEEK | MSG_DONTWAIT);
        delay(10);
    }
    if (bannerPeek <= 0) {
        client.stop();
        libssh2_exit();
        return {false, "", "", "TCP connected to " + host + ":" + String(port) +
                                 ", but the SSH server banner did not arrive"};
    }
    if (bannerFirstByte != 'S') {
        client.stop();
        libssh2_exit();
        return {false, "", "", "TCP endpoint " + host + ":" + String(port) +
                                 " did not begin with an SSH banner"};
    }

    SshAllocatorState allocator = {0, allocationCapabilities};
    LIBSSH2_SESSION* session = libssh2_session_init_ex(
        allocateSshMemory, releaseSshMemory, reallocateSshMemory, &allocator);
    if (session == nullptr) {
        client.stop();
        libssh2_exit();
        return {false, "", "", "libssh2 could not allocate an SSH session; "
                                 "requested " + String(allocator.failedAllocationBytes) +
                                 " bytes with " + String(freeAllocatorMemory) + " bytes free in " +
                                 (freePsram > 0 ? String("PSRAM") : String("internal RAM"))};
    }
    libssh2_session_set_blocking(session, 0);
    const int preference = libssh2_session_method_pref(
        session, LIBSSH2_METHOD_KEX, "diffie-hellman-group-exchange-sha256");
    if (preference != 0) {
        libssh2_session_free(session);
        client.stop();
        libssh2_exit();
        return {false, "", "", "libssh2 could not select diffie-hellman-group-exchange-sha256; "
                                 "error code " + String(preference)};
    }
    const std::uint32_t handshakeDeadline = millis() + timeoutMs;
    int handshake = LIBSSH2_ERROR_EAGAIN;
    while (handshake == LIBSSH2_ERROR_EAGAIN &&
           static_cast<std::int32_t>(handshakeDeadline - millis()) > 0) {
        handshake = libssh2_session_handshake(session, client.fd());
        delay(5);
    }
    if (handshake == LIBSSH2_ERROR_EAGAIN) {
        handshake = LIBSSH2_ERROR_TIMEOUT;
    }
    if (handshake != 0) {
        const int startupState = static_cast<int>(session->startup_state);
        const int keyExchangeState = static_cast<int>(session->startup_key_state.state);
        const int keyExchangeLowState =
            static_cast<int>(session->startup_key_state.key_state_low.state);
        const String negotiatedKex = session->kex != nullptr && session->kex->name != nullptr
            ? String(session->kex->name) : String("none");
        const std::uint32_t packetLength = session->packet.packet_length;
        const std::size_t packetBytes = session->packet.total_num;
        const std::size_t packetWriteIndex = session->packet.writeidx;
        char* detail = nullptr;
        int detailLength = 0;
        libssh2_session_last_error(session, &detail, &detailLength, 0);
        const String reason = detail != nullptr && detailLength > 0
            ? String(detail).substring(0, static_cast<unsigned int>(detailLength))
            : String("no protocol detail");
        libssh2_session_free(session);
        client.stop();
        libssh2_exit();
        return {false, "", "", "SSH handshake with " + host + ":" + String(port) +
                                 " failed with code " + String(handshake) + ": " + reason +
                                 " (startup=" + String(startupState) +
                                 ", kex=" + String(keyExchangeState) +
                                 ", low=" + String(keyExchangeLowState) +
                                 ", method=" + negotiatedKex +
                                 ", packet_len=" + String(packetLength) +
                                 ", packet_bytes=" + String(packetBytes) +
                                 ", packet_write=" + String(packetWriteIndex) + ")"};
    }

    String fingerprint;
    String type;
    const OperationResult hostKeyResult = formatSessionHostKey(session, fingerprint, type);
    if (!hostKeyResult.success) {
        libssh2_session_disconnect(session, "CardMind host-key probe complete");
        libssh2_session_free(session);
        client.stop();
        libssh2_exit();
        return {false, "", "", hostKeyResult.error};
    }

    libssh2_session_disconnect(session, "CardMind host-key probe complete");
    libssh2_session_free(session);
    client.stop();
    libssh2_exit();
    return {true, fingerprint, type, ""};
}

struct SshClient::Implementation {
    SshAllocatorState allocator;
    NetworkClient network;
    LIBSSH2_SESSION* session;
    LIBSSH2_CHANNEL* channel;
    LIBSSH2_SFTP* sftp;
    bool runtimeInitialized;
    String hostFingerprint;
    String hostKeyType;

    Implementation()
        : allocator{0, static_cast<std::uint32_t>(MALLOC_CAP_8BIT)},
          session(nullptr),
          channel(nullptr),
          sftp(nullptr),
          runtimeInitialized(false)
    {
    }
};

SshClient::SshClient()
    : implementation_(new (std::nothrow) Implementation())
{
}

SshClient::~SshClient()
{
    close();
    delete implementation_;
    implementation_ = nullptr;
}

OperationResult SshClient::connect(const SshProfile& profile, std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr) {
        return {false, "Failed to allocate SSH client state"};
    }
    if (!sshProfileIsComplete(profile)) {
        return {false, "SSH profile is incomplete; configure host, user, and authentication"};
    }
    if (timeoutMs < 1000 || timeoutMs > 120000) {
        return {false, "SSH timeout must be between 1000 and 120000 ms"};
    }
    close();
    if (libssh2_init(0) != 0) {
        return {false, "libssh2 initialization failed"};
    }
    implementation_->runtimeInitialized = true;
    implementation_->network.setTimeout(timeoutMs);
    if (!implementation_->network.connect(profile.host.c_str(), profile.port,
                                           static_cast<int32_t>(timeoutMs))) {
        close();
        return {false, "TCP connection to SSH host failed"};
    }
    implementation_->allocator.failedAllocationBytes = 0;
    const std::size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    implementation_->allocator.capabilities = freePsram > 0
        ? static_cast<std::uint32_t>(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
        : static_cast<std::uint32_t>(MALLOC_CAP_8BIT);
    implementation_->session = libssh2_session_init_ex(
        allocateSshMemory, releaseSshMemory, reallocateSshMemory,
        &implementation_->allocator);
    if (implementation_->session == nullptr) {
        const std::size_t requested = implementation_->allocator.failedAllocationBytes;
        close();
        return {false, "SSH session allocation failed; requested " + String(requested) +
                       " bytes"};
    }
    libssh2_session_set_blocking(implementation_->session, 0);
    const int preference = libssh2_session_method_pref(
        implementation_->session, LIBSSH2_METHOD_KEX,
        "curve25519-sha256,ecdh-sha2-nistp256,diffie-hellman-group-exchange-sha256");
    if (preference != 0) {
        const String error = sessionError(
            implementation_->session, "SSH KEX preference", preference);
        close();
        return {false, error};
    }
    const int handshake = runUntilComplete(
        [this]() {
            return libssh2_session_handshake(
                implementation_->session, implementation_->network.fd());
        }, timeoutMs);
    if (handshake != 0) {
        const String error = sessionError(
            implementation_->session, "SSH handshake", handshake);
        close();
        return {false, error};
    }
    const OperationResult hostKey = formatSessionHostKey(
        implementation_->session, implementation_->hostFingerprint,
        implementation_->hostKeyType);
    if (!hostKey.success) {
        close();
        return hostKey;
    }

    return {true, ""};
}

OperationResult SshClient::authenticate(const SshProfile& profile,
                                        std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    int authentication = 0;
    if (profile.authMode == SshAuthMode::Password) {
        authentication = runUntilComplete(
            [this, &profile]() {
                return libssh2_userauth_password_ex(
                    implementation_->session, profile.username.c_str(),
                    static_cast<unsigned int>(profile.username.length()),
                    profile.password.c_str(),
                    static_cast<unsigned int>(profile.password.length()), nullptr);
            }, timeoutMs);
    } else {
        const char* passphrase = profile.privateKeyPassphrase.isEmpty()
            ? nullptr : profile.privateKeyPassphrase.c_str();
        authentication = runUntilComplete(
            [this, &profile, passphrase]() {
                return libssh2_userauth_publickey_fromfile_ex(
                    implementation_->session, profile.username.c_str(),
                    static_cast<unsigned int>(profile.username.length()), nullptr,
                    kSshPrivateKeyVfsPath, passphrase);
            }, timeoutMs);
    }
    if (authentication != 0) {
        const String error = sessionError(
            implementation_->session, "SSH authentication", authentication);
        close();
        return {false, error};
    }
    return {true, ""};
}

OperationResult SshClient::openTerminal(std::uint32_t columns, std::uint32_t rows,
                                        std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    if (columns < 20 || columns > 240 || rows < 4 || rows > 100) {
        return {false, "SSH terminal dimensions are out of range"};
    }
    const std::uint32_t deadline = millis() + timeoutMs;
    while (implementation_->channel == nullptr &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        implementation_->channel = libssh2_channel_open_ex(
            implementation_->session, "session", sizeof("session") - 1,
            kTerminalWindowBytes, kTerminalPacketBytes, nullptr, 0);
        if (implementation_->channel == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) {
            const int errorCode = libssh2_session_last_errno(implementation_->session);
            return {false, sessionError(implementation_->session,
                                        "SSH channel open", errorCode)};
        }
        delay(5);
    }
    if (implementation_->channel == nullptr) {
        return {false, "SSH channel open timed out"};
    }
    const int pty = runUntilComplete(
        [this, columns, rows]() {
            return libssh2_channel_request_pty_ex(
                implementation_->channel, "xterm", 5, nullptr, 0,
                static_cast<int>(columns), static_cast<int>(rows), 0, 0);
        }, timeoutMs);
    if (pty != 0) {
        return {false, sessionError(implementation_->session,
                                    "SSH PTY request", pty)};
    }
    const int shell = runUntilComplete(
        [this]() { return libssh2_channel_shell(implementation_->channel); },
        timeoutMs);
    if (shell != 0) {
        return {false, sessionError(implementation_->session,
                                    "SSH shell request", shell)};
    }
    return {true, ""};
}

OperationResult SshClient::resizeTerminal(std::uint32_t columns,
                                          std::uint32_t rows,
                                          std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->channel == nullptr) {
        return {false, "SSH terminal is not open"};
    }
    if (columns < 20 || columns > 240 || rows < 4 || rows > 100) {
        return {false, "SSH terminal dimensions are out of range"};
    }
    const int result = runUntilComplete(
        [this, columns, rows]() {
            return libssh2_channel_request_pty_size_ex(
                implementation_->channel, static_cast<int>(columns),
                static_cast<int>(rows), 0, 0);
        }, timeoutMs);
    return result == 0
        ? OperationResult{true, ""}
        : OperationResult{false, sessionError(
              implementation_->session, "SSH PTY resize", result)};
}

int SshClient::read(std::uint8_t* output, std::size_t maximumBytes)
{
    if (implementation_ == nullptr || implementation_->channel == nullptr ||
        output == nullptr || maximumBytes == 0) {
        return LIBSSH2_ERROR_BAD_USE;
    }
    const ssize_t readBytes = libssh2_channel_read_ex(
        implementation_->channel, 0, reinterpret_cast<char*>(output), maximumBytes);
    if (readBytes == LIBSSH2_ERROR_EAGAIN) {
        return 0;
    }
    return static_cast<int>(readBytes);
}

OperationResult SshClient::write(const std::uint8_t* data, std::size_t bytes,
                                 std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->channel == nullptr) {
        return {false, "SSH terminal is not open"};
    }
    if (data == nullptr || bytes == 0) {
        return {false, "SSH terminal write requires non-empty data"};
    }
    std::size_t written = 0;
    const std::uint32_t deadline = millis() + timeoutMs;
    while (written < bytes && static_cast<std::int32_t>(deadline - millis()) > 0) {
        const ssize_t result = libssh2_channel_write_ex(
            implementation_->channel, 0,
            reinterpret_cast<const char*>(data + written), bytes - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
        } else if (result != LIBSSH2_ERROR_EAGAIN) {
            return {false, sessionError(implementation_->session,
                                        "SSH terminal write", static_cast<int>(result))};
        }
        delay(2);
    }
    return written == bytes
        ? OperationResult{true, ""}
        : OperationResult{false, "SSH terminal write timed out after " +
                                 String(written) + " of " + String(bytes) + " bytes"};
}

OperationResult SshClient::executeCommand(const String& command,
                                          std::string& output,
                                          int& exitStatus,
                                          std::size_t maximumOutputBytes,
                                          std::uint32_t timeoutMs)
{
    output.clear();
    exitStatus = -1;
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    if (implementation_->channel != nullptr) {
        return {false, "SSH session already has an open channel"};
    }
    if (command.isEmpty() || command.length() > 1024 || command.indexOf('\0') >= 0) {
        return {false, "SSH command must contain 1 to 1024 bytes without NUL characters"};
    }
    if (maximumOutputBytes == 0 || maximumOutputBytes > 16384) {
        return {false, "SSH command output limit must be between 1 and 16384 bytes"};
    }
    if (timeoutMs < 1000 || timeoutMs > 120000) {
        return {false, "SSH command timeout must be between 1000 and 120000 ms"};
    }
    const std::uint32_t deadline = millis() + timeoutMs;
    while (implementation_->channel == nullptr &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        implementation_->channel = libssh2_channel_open_session(implementation_->session);
        if (implementation_->channel == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) {
            const int errorCode = libssh2_session_last_errno(implementation_->session);
            return {false, sessionError(implementation_->session,
                                        "SSH command channel open", errorCode)};
        }
        delay(5);
    }
    if (implementation_->channel == nullptr) {
        return {false, "SSH command channel open timed out"};
    }
    const int started = runUntilComplete(
        [this, &command]() {
            return libssh2_channel_exec(implementation_->channel, command.c_str());
        }, timeoutMs);
    if (started != 0) {
        return {false, sessionError(implementation_->session,
                                    "SSH command execution", started)};
    }
    std::uint8_t buffer[1024] = {};
    const std::uint32_t readDeadline = millis() + timeoutMs;
    while (static_cast<std::int32_t>(readDeadline - millis()) > 0) {
        bool progressed = false;
        for (int streamId = 0; streamId <= 1; ++streamId) {
            const ssize_t bytes = libssh2_channel_read_ex(
                implementation_->channel, streamId,
                reinterpret_cast<char*>(buffer), sizeof(buffer));
            if (bytes > 0) {
                const std::size_t count = static_cast<std::size_t>(bytes);
                if (count > maximumOutputBytes - output.size()) {
                    return {false, "SSH command output exceeded the configured 16384-byte limit"};
                }
                output.append(reinterpret_cast<const char*>(buffer), count);
                progressed = true;
            } else if (bytes < 0 && bytes != LIBSSH2_ERROR_EAGAIN) {
                return {false, sessionError(implementation_->session,
                                            streamId == 0 ? "SSH command stdout read"
                                                          : "SSH command stderr read",
                                            static_cast<int>(bytes))};
            }
        }
        if (libssh2_channel_eof(implementation_->channel) != 0) {
            exitStatus = libssh2_channel_get_exit_status(implementation_->channel);
            return {true, ""};
        }
        if (!progressed) {
            delay(5);
        }
    }
    return {false, "SSH command timed out before the remote channel closed"};
}

OperationResult SshClient::openSftp(std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    if (implementation_->sftp != nullptr) {
        return {true, ""};
    }
    const std::uint32_t deadline = millis() + timeoutMs;
    while (implementation_->sftp == nullptr &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        implementation_->sftp = libssh2_sftp_init(implementation_->session);
        if (implementation_->sftp == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) {
            const int errorCode = libssh2_session_last_errno(implementation_->session);
            return {false, sessionError(implementation_->session,
                                        "SFTP initialization", errorCode)};
        }
        delay(5);
    }
    return implementation_->sftp != nullptr
        ? OperationResult{true, ""}
        : OperationResult{false, "SFTP initialization timed out"};
}

SftpEntriesResult SshClient::listSftpDirectory(const String& path,
                                               std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr) {
        return {false, {}, "SFTP session is not open"};
    }
    if (!isValidRemotePath(path)) {
        return {false, {}, "SFTP directory path must be absolute and at most 511 bytes"};
    }
    LIBSSH2_SFTP_HANDLE* directory = nullptr;
    const std::uint32_t openDeadline = millis() + timeoutMs;
    while (directory == nullptr && static_cast<std::int32_t>(openDeadline - millis()) > 0) {
        directory = libssh2_sftp_opendir(implementation_->sftp, path.c_str());
        if (directory == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) {
            return {false, {}, sessionError(implementation_->session,
                                             "SFTP directory open", libssh2_session_last_errno(
                                                 implementation_->session))};
        }
        delay(5);
    }
    if (directory == nullptr) {
        return {false, {}, "SFTP directory open timed out"};
    }
    std::vector<SftpEntry> entries;
    const std::uint32_t deadline = millis() + timeoutMs;
    bool completed = false;
    while (static_cast<std::int32_t>(deadline - millis()) > 0) {
        char name[256] = {};
        LIBSSH2_SFTP_ATTRIBUTES attributes = {};
        const ssize_t result = libssh2_sftp_readdir_ex(
            directory, name, sizeof(name) - 1, nullptr, 0, &attributes);
        if (result > 0) {
            const String entryName = String(name).substring(0, static_cast<unsigned int>(result));
            if (entryName != "." && entryName != ".." &&
                entryName.indexOf('/') < 0 && entryName.indexOf('\r') < 0 &&
                entryName.indexOf('\n') < 0) {
                const bool directoryEntry =
                    (attributes.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0 &&
                    LIBSSH2_SFTP_S_ISDIR(attributes.permissions);
                entries.push_back({entryName, directoryEntry,
                                   static_cast<std::uint64_t>(attributes.filesize)});
            }
            continue;
        }
        if (result == LIBSSH2_ERROR_EAGAIN) {
            delay(5);
            continue;
        }
        if (result < 0) {
            closeSftpHandle(directory);
            return {false, {}, sessionError(implementation_->session,
                                             "SFTP directory read", static_cast<int>(result))};
        }
        completed = true;
        break;
    }
    closeSftpHandle(directory);
    if (!completed) {
        return {false, {}, "SFTP directory listing timed out before end-of-list"};
    }
    std::sort(entries.begin(), entries.end(), [](const SftpEntry& left,
                                                  const SftpEntry& right) {
        if (left.directory != right.directory) {
            return left.directory;
        }
        return left.name.compareTo(right.name) < 0;
    });
    return {true, std::move(entries), ""};
}

OperationResult SshClient::downloadSftpFile(const String& remotePath,
                                            const String& workspaceName,
                                            std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr) {
        return {false, "SFTP session is not open"};
    }
    if (!isValidRemotePath(remotePath)) {
        return {false, "SFTP remote file path is invalid"};
    }
    const String localPath = workspaceFilePath(workspaceName);
    if (localPath.isEmpty()) {
        return {false, "SFTP download destination filename is invalid"};
    }
    LIBSSH2_SFTP_HANDLE* remote = nullptr;
    const std::uint32_t openDeadline = millis() + timeoutMs;
    while (remote == nullptr && static_cast<std::int32_t>(openDeadline - millis()) > 0) {
        remote = libssh2_sftp_open(implementation_->sftp, remotePath.c_str(),
                                   LIBSSH2_FXF_READ, 0);
        if (remote == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) break;
        delay(5);
    }
    if (remote == nullptr) {
        return {false, sessionError(implementation_->session, "SFTP remote file open",
                                    libssh2_session_last_errno(implementation_->session))};
    }
    const String temporaryPath = localPath + ".sftp-part";
    SD.remove(temporaryPath);
    File local = SD.open(temporaryPath, FILE_WRITE);
    if (!local) {
        closeSftpHandle(remote);
        return {false, "Failed to create the SFTP download file on microSD"};
    }
    std::size_t total = 0;
    std::uint8_t buffer[1024] = {};
    std::uint32_t deadline = millis() + timeoutMs;
    bool completed = false;
    while (static_cast<std::int32_t>(deadline - millis()) > 0) {
        const ssize_t readBytes = libssh2_sftp_read(
            remote, reinterpret_cast<char*>(buffer), sizeof(buffer));
        if (readBytes > 0) {
            total += static_cast<std::size_t>(readBytes);
            if (total > kMaximumWorkspaceFileBytes ||
                local.write(buffer, static_cast<std::size_t>(readBytes)) !=
                    static_cast<std::size_t>(readBytes)) {
                local.close();
                closeSftpHandle(remote);
                SD.remove(temporaryPath);
                return {false, total > kMaximumWorkspaceFileBytes
                    ? String("SFTP file exceeds the 491520-byte workspace limit")
                    : String("microSD rejected SFTP download data")};
            }
            deadline = millis() + timeoutMs;
        } else if (readBytes == LIBSSH2_ERROR_EAGAIN) {
            delay(5);
        } else if (readBytes == 0) {
            completed = true;
            break;
        } else {
            local.close();
            closeSftpHandle(remote);
            SD.remove(temporaryPath);
            return {false, sessionError(implementation_->session, "SFTP file read",
                                        static_cast<int>(readBytes))};
        }
    }
    local.flush();
    local.close();
    closeSftpHandle(remote);
    if (!completed) {
        SD.remove(temporaryPath);
        return {false, "SFTP download timed out before remote end-of-file"};
    }
    if (SD.exists(localPath) && !SD.remove(localPath)) {
        SD.remove(temporaryPath);
        return {false, "Failed to replace the existing workspace file"};
    }
    if (!SD.rename(temporaryPath, localPath)) {
        SD.remove(temporaryPath);
        return {false, "Failed to activate the downloaded workspace file"};
    }
    return {true, ""};
}

OperationResult SshClient::uploadSftpFile(const String& workspaceName,
                                          const String& remotePath,
                                          std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr) {
        return {false, "SFTP session is not open"};
    }
    if (!isValidRemotePath(remotePath)) {
        return {false, "SFTP remote file path is invalid"};
    }
    const String localPath = workspaceFilePath(workspaceName);
    File local = SD.open(localPath, FILE_READ);
    if (!local || local.isDirectory()) {
        if (local) local.close();
        return {false, "SFTP upload source could not be opened from the workspace"};
    }
    LIBSSH2_SFTP_HANDLE* remote = nullptr;
    const std::uint32_t openDeadline = millis() + timeoutMs;
    while (remote == nullptr && static_cast<std::int32_t>(openDeadline - millis()) > 0) {
        remote = libssh2_sftp_open(
            implementation_->sftp, remotePath.c_str(),
            LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
            LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR);
        if (remote == nullptr &&
            libssh2_session_last_errno(implementation_->session) != LIBSSH2_ERROR_EAGAIN) break;
        delay(5);
    }
    if (remote == nullptr) {
        local.close();
        return {false, sessionError(implementation_->session, "SFTP remote file create",
                                    libssh2_session_last_errno(implementation_->session))};
    }
    std::uint8_t buffer[1024] = {};
    while (local.available()) {
        const std::size_t readBytes = local.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            local.close();
            closeSftpHandle(remote);
            libssh2_sftp_unlink(implementation_->sftp, remotePath.c_str());
            return {false, "microSD returned no data before the SFTP upload reached end-of-file"};
        }
        std::size_t written = 0;
        const std::uint32_t deadline = millis() + timeoutMs;
        while (written < readBytes && static_cast<std::int32_t>(deadline - millis()) > 0) {
            const ssize_t result = libssh2_sftp_write(
                remote, reinterpret_cast<const char*>(buffer + written), readBytes - written);
            if (result > 0) written += static_cast<std::size_t>(result);
            else if (result != LIBSSH2_ERROR_EAGAIN) {
                local.close();
                closeSftpHandle(remote);
                libssh2_sftp_unlink(implementation_->sftp, remotePath.c_str());
                return {false, sessionError(implementation_->session, "SFTP file write",
                                            static_cast<int>(result))};
            }
            delay(2);
        }
        if (written != readBytes) {
            local.close();
            closeSftpHandle(remote);
            libssh2_sftp_unlink(implementation_->sftp, remotePath.c_str());
            return {false, "SFTP upload timed out"};
        }
    }
    local.close();
    closeSftpHandle(remote);
    return {true, ""};
}

OperationResult SshClient::createSftpDirectory(const String& path,
                                               std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr ||
        !isValidRemotePath(path)) {
        return {false, "SFTP directory path or session is invalid"};
    }
    const int result = runUntilComplete([this, &path]() {
        return libssh2_sftp_mkdir(implementation_->sftp, path.c_str(), 0700);
    }, timeoutMs);
    return result == 0 ? OperationResult{true, ""}
        : OperationResult{false, sessionError(implementation_->session,
                                              "SFTP directory create", result)};
}

OperationResult SshClient::removeSftpPath(const String& path, bool directory,
                                          std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr ||
        !isValidRemotePath(path)) {
        return {false, "SFTP removal path or session is invalid"};
    }
    const int result = runUntilComplete([this, &path, directory]() {
        return directory ? libssh2_sftp_rmdir(implementation_->sftp, path.c_str())
                         : libssh2_sftp_unlink(implementation_->sftp, path.c_str());
    }, timeoutMs);
    return result == 0 ? OperationResult{true, ""}
        : OperationResult{false, sessionError(implementation_->session,
                                              "SFTP remove", result)};
}

OperationResult SshClient::renameSftpPath(const String& sourcePath,
                                          const String& destinationPath,
                                          std::uint32_t timeoutMs)
{
    if (implementation_ == nullptr || implementation_->sftp == nullptr ||
        !isValidRemotePath(sourcePath) || !isValidRemotePath(destinationPath)) {
        return {false, "SFTP rename paths or session are invalid"};
    }
    const int result = runUntilComplete([this, &sourcePath, &destinationPath]() {
        return libssh2_sftp_rename(implementation_->sftp, sourcePath.c_str(),
                                   destinationPath.c_str());
    }, timeoutMs);
    return result == 0 ? OperationResult{true, ""}
        : OperationResult{false, sessionError(implementation_->session,
                                              "SFTP rename", result)};
}

bool SshClient::isOpen() const
{
    return implementation_ != nullptr && implementation_->channel != nullptr &&
           libssh2_channel_eof(implementation_->channel) == 0;
}

bool SshClient::isSftpOpen() const
{
    return implementation_ != nullptr && implementation_->sftp != nullptr;
}

String SshClient::fingerprint() const
{
    return implementation_ == nullptr ? String() : implementation_->hostFingerprint;
}

String SshClient::hostKeyType() const
{
    return implementation_ == nullptr ? String() : implementation_->hostKeyType;
}

void SshClient::close()
{
    if (implementation_ == nullptr) {
        return;
    }
    if (implementation_->channel != nullptr) {
        LIBSSH2_CHANNEL* channel = implementation_->channel;
        runUntilComplete([channel]() { return libssh2_channel_free(channel); }, 5000);
        implementation_->channel = nullptr;
    }
    if (implementation_->sftp != nullptr) {
        LIBSSH2_SFTP* sftp = implementation_->sftp;
        runUntilComplete([sftp]() { return libssh2_sftp_shutdown(sftp); }, 5000);
        implementation_->sftp = nullptr;
    }
    if (implementation_->session != nullptr) {
        LIBSSH2_SESSION* session = implementation_->session;
        runUntilComplete([session]() { return libssh2_session_free(session); }, 5000);
        implementation_->session = nullptr;
    }
    implementation_->network.stop();
    implementation_->hostFingerprint = "";
    implementation_->hostKeyType = "";
    if (implementation_->runtimeInitialized) {
        libssh2_exit();
        implementation_->runtimeInitialized = false;
    }
}

}  // namespace cardputer
