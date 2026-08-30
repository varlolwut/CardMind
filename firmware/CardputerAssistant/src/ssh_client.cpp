#include "ssh_client.h"

#include "ssh_command_options.h"

#include "file_workspace.h"
#include "sd_storage.h"
#include "text_utils.h"

#include <libssh2.h>
#include <libssh2_sftp.h>
#define public public_key
#include <libssh2_priv.h>
#undef public

#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>

#include <NetworkClient.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>
#include <sys/socket.h>
#include <vector>

namespace cardputer {
namespace {

constexpr const char* kSshNamespace = "cardmind_ssh";
constexpr const char* kSshDirectory = "/assistant/ssh";
constexpr const char* kLegacySshPrivateKeyPath = "/assistant/ssh/id.pem";
constexpr const char* kLegacySshPrivateKeyOwnerKey = "legacy_key";
constexpr const char* kSshKnownHostsPath = "/assistant/ssh/known_hosts";
constexpr const char* kSshKnownHostsTemporaryPath = "/assistant/ssh/known_hosts.tmp";
constexpr std::size_t kMaximumKnownHostsBytes = 16384;
constexpr std::size_t kMinimumSshPrivateKeyBytes = 64;
constexpr std::size_t kMaximumSshPrivateKeyBytes = 16384;
constexpr std::size_t kSshPrivateKeyRecordHeaderBytes = sizeof(std::uint64_t);
constexpr std::size_t kMaximumSshPrivateKeyRecordSlots = kMaximumSshProfiles + 1;
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
    if (profile.authMode == SshAuthMode::PrivateKey &&
        !sshPrivateKeyIsInstalled(profile.privateKeyId)) {
        return {false, "SSH private-key authentication requires an installed key"};
    }
    return {true, ""};
}

String indexedProfileIdKey(std::size_t index)
{
    return String("i") + String(index);
}

String indexedPrivateKeyIdKey(std::size_t index)
{
    return String("q") + String(index);
}

String sshPrivateKeyRecordIdKey(std::size_t index)
{
    return String("d") + String(index);
}

String sshPrivateKeyRecordBlobKey(std::size_t index)
{
    return String("r") + String(index);
}

struct SshPrivateKeyRecordSlot {
    bool idPresent;
    std::uint64_t id;
    bool blobPresent;
    std::size_t blobBytes;
};

using SshPrivateKeyRecordSlots =
    std::array<SshPrivateKeyRecordSlot, kMaximumSshPrivateKeyRecordSlots>;

void clearSshSecretBytes(std::vector<std::uint8_t>& bytes)
{
    if (!bytes.empty()) {
        mbedtls_platform_zeroize(bytes.data(), bytes.size());
    }
    bytes.clear();
}

std::uint64_t decodeSshPrivateKeyId(const std::uint8_t* bytes)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
    return value;
}

void encodeSshPrivateKeyId(std::uint64_t value, std::uint8_t* bytes)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

OperationResult readOptionalSshPrivateKeyId(Preferences& preferences,
                                            const String& key,
                                            std::uint64_t& value,
                                            bool& found)
{
    const PreferenceType type = preferences.getType(key.c_str());
    if (type == PT_INVALID) {
        value = 0;
        found = false;
        return {true, ""};
    }
    if (type != PT_U64) {
        return {false, "Stored SSH private-key reference has an invalid NVS type"};
    }
    value = preferences.getULong64(key.c_str(), 0);
    found = true;
    return value == 0
        ? OperationResult{false, "Stored SSH private-key reference is zero"}
        : OperationResult{true, ""};
}

OperationResult readSshPrivateKeyRecordSlots(
    Preferences& preferences,
    SshPrivateKeyRecordSlots& slots)
{
    for (std::size_t index = 0; index < slots.size(); ++index) {
        SshPrivateKeyRecordSlot slot = {false, 0, false, 0};
        const OperationResult idResult = readOptionalSshPrivateKeyId(
            preferences, sshPrivateKeyRecordIdKey(index), slot.id, slot.idPresent);
        if (!idResult.success) {
            return idResult;
        }
        const String blobKey = sshPrivateKeyRecordBlobKey(index);
        const PreferenceType blobType = preferences.getType(blobKey.c_str());
        if (blobType != PT_INVALID && blobType != PT_BLOB) {
            return {false, "Stored SSH private-key record has an invalid NVS type"};
        }
        slot.blobPresent = blobType == PT_BLOB;
        if (slot.blobPresent) {
            slot.blobBytes = preferences.getBytesLength(blobKey.c_str());
            if (slot.blobBytes < kSshPrivateKeyRecordHeaderBytes +
                                     kMinimumSshPrivateKeyBytes ||
                slot.blobBytes > kSshPrivateKeyRecordHeaderBytes +
                                     kMaximumSshPrivateKeyBytes) {
                return {false, "Stored SSH private-key record has an invalid length"};
            }
        }
        slots[index] = slot;
    }
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (!slots[index].idPresent) {
            continue;
        }
        for (std::size_t other = index + 1; other < slots.size(); ++other) {
            if (slots[other].idPresent && slots[index].id == slots[other].id) {
                return {false, "Stored SSH private-key record IDs are not unique"};
            }
        }
    }
    return {true, ""};
}

OperationResult readSshProfilePrivateKeyIds(
    Preferences& preferences,
    std::vector<std::uint64_t>& privateKeyIds)
{
    const PreferenceType countType = preferences.getType("cnt");
    if (countType != PT_INVALID && countType != PT_U8) {
        return {false, "Stored SSH profile count has an invalid NVS type"};
    }
    const std::size_t storedCount = countType == PT_U8
        ? preferences.getUChar("cnt", 0) : 0;
    if (storedCount > kMaximumSshProfiles) {
        return {false, "Stored SSH profile count exceeds the supported limit"};
    }
    std::size_t logicalCount = storedCount;
    if (storedCount == 0) {
        const PreferenceType legacyHostType = preferences.getType("host");
        if (legacyHostType != PT_INVALID && legacyHostType != PT_STR) {
            return {false, "Stored legacy SSH profile authority has an invalid NVS type"};
        }
        if (legacyHostType == PT_STR &&
            !preferences.getString("host", "").isEmpty()) {
            logicalCount = 1;
        }
    }
    if (logicalCount == 0) {
        for (std::size_t index = 0; index < kMaximumSshProfiles; ++index) {
            if (preferences.getType(indexedPrivateKeyIdKey(index).c_str()) !=
                PT_INVALID) {
                return {false, "Stored SSH private-key references have no profile authority"};
            }
        }
    }
    std::vector<std::uint64_t> loaded;
    loaded.reserve(logicalCount);
    for (std::size_t index = 0; index < logicalCount; ++index) {
        std::uint64_t id = 0;
        bool found = false;
        const OperationResult result = readOptionalSshPrivateKeyId(
            preferences, indexedPrivateKeyIdKey(index), id, found);
        if (!result.success) {
            return result;
        }
        loaded.push_back(found ? id : 0);
    }
    privateKeyIds = std::move(loaded);
    return {true, ""};
}

bool sshPrivateKeyIdIsReferenced(
    std::uint64_t id,
    const std::vector<std::uint64_t>& profileIds,
    std::uint64_t legacyOwnerId)
{
    return id == legacyOwnerId ||
        std::find(profileIds.begin(), profileIds.end(), id) != profileIds.end();
}

bool removeExactSshPreference(Preferences& preferences, const String& key)
{
    if (preferences.getType(key.c_str()) == PT_INVALID) {
        return true;
    }
    return preferences.remove(key.c_str()) &&
        preferences.getType(key.c_str()) == PT_INVALID;
}

OperationResult cleanupSshPrivateKeyRecords()
{
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH private-key records for cleanup"};
    }
    std::vector<std::uint64_t> profileIds;
    OperationResult result = readSshProfilePrivateKeyIds(preferences, profileIds);
    std::uint64_t legacyOwnerId = 0;
    bool legacyOwnerFound = false;
    if (result.success) {
        result = readOptionalSshPrivateKeyId(
            preferences, kLegacySshPrivateKeyOwnerKey,
            legacyOwnerId, legacyOwnerFound);
    }
    SshPrivateKeyRecordSlots slots = {};
    if (result.success) {
        result = readSshPrivateKeyRecordSlots(preferences, slots);
    }
    if (!result.success) {
        preferences.end();
        return result;
    }
    if (legacyOwnerFound &&
        std::find(profileIds.begin(), profileIds.end(), legacyOwnerId) !=
            profileIds.end()) {
        if (!removeExactSshPreference(
                preferences, kLegacySshPrivateKeyOwnerKey)) {
            preferences.end();
            return {false, "Failed to clear the claimed legacy SSH key owner"};
        }
        legacyOwnerFound = false;
        legacyOwnerId = 0;
    }
    for (const std::uint64_t id : profileIds) {
        if (id == 0) {
            continue;
        }
        bool complete = false;
        for (const SshPrivateKeyRecordSlot& slot : slots) {
            complete = complete || (slot.idPresent && slot.blobPresent && slot.id == id);
        }
        if (!complete) {
            preferences.end();
            return {false, "An SSH profile references a missing private-key record"};
        }
    }
    if (legacyOwnerFound) {
        bool complete = false;
        for (const SshPrivateKeyRecordSlot& slot : slots) {
            complete = complete ||
                (slot.idPresent && slot.blobPresent && slot.id == legacyOwnerId);
        }
        if (!complete) {
            preferences.end();
            return {false, "Legacy SSH key owner references a missing record"};
        }
    }
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const SshPrivateKeyRecordSlot& slot = slots[index];
        const bool referenced = slot.idPresent && sshPrivateKeyIdIsReferenced(
            slot.id, profileIds, legacyOwnerFound ? legacyOwnerId : 0);
        bool removed = true;
        if (slot.idPresent && !referenced) {
            removed = removeExactSshPreference(
                preferences, sshPrivateKeyRecordIdKey(index));
        }
        if (slot.blobPresent && (!slot.idPresent || !referenced)) {
            removed = removeExactSshPreference(
                preferences, sshPrivateKeyRecordBlobKey(index)) && removed;
        }
        if (!removed) {
            preferences.end();
            return {false, "Failed to remove an unreferenced SSH private-key record"};
        }
    }
    preferences.end();
    return {true, ""};
}

bool sshPrivateKeyPemIsValid(const std::uint8_t* bytes, std::size_t size)
{
    if (bytes == nullptr || size < kMinimumSshPrivateKeyBytes ||
        size > kMaximumSshPrivateKeyBytes) {
        return false;
    }
    constexpr char marker[] = "-----BEGIN ";
    const std::size_t inspected = std::min<std::size_t>(size, 64);
    for (std::size_t offset = 0;
         offset + sizeof(marker) - 1 <= inspected; ++offset) {
        if (std::memcmp(bytes + offset, marker, sizeof(marker) - 1) == 0) {
            return true;
        }
    }
    return false;
}

OperationResult readSshPrivateKeyFile(
    const String& path,
    std::vector<std::uint8_t>& record)
{
    File source = SD.open(path, FILE_READ);
    if (!source || source.isDirectory()) {
        if (source) {
            source.close();
        }
        return {false, "SSH private-key upload could not be opened"};
    }
    const std::size_t size = source.size();
    if (size < kMinimumSshPrivateKeyBytes || size > kMaximumSshPrivateKeyBytes) {
        source.close();
        return {false, "SSH private key must contain 64 to 16384 bytes"};
    }
    std::vector<std::uint8_t> loaded(
        kSshPrivateKeyRecordHeaderBytes + size, 0);
    std::size_t copied = 0;
    while (copied < size) {
        const std::size_t readBytes = source.read(
            loaded.data() + kSshPrivateKeyRecordHeaderBytes + copied,
            size - copied);
        if (readBytes == 0) {
            source.close();
            clearSshSecretBytes(loaded);
            return {false, "Failed while reading the complete SSH private key"};
        }
        copied += readBytes;
    }
    source.close();
    if (!sshPrivateKeyPemIsValid(
            loaded.data() + kSshPrivateKeyRecordHeaderBytes, size)) {
        clearSshSecretBytes(loaded);
        return {false, "SSH private key is not a PEM document"};
    }
    record = std::move(loaded);
    return {true, ""};
}

std::uint64_t generateUniqueSshPrivateKeyId(
    const SshPrivateKeyRecordSlots& slots)
{
    for (std::size_t attempt = 0; attempt < 32; ++attempt) {
        const std::uint64_t candidate =
            (static_cast<std::uint64_t>(esp_random()) << 32) |
            static_cast<std::uint64_t>(esp_random());
        if (candidate == 0) {
            continue;
        }
        bool collision = false;
        for (const SshPrivateKeyRecordSlot& slot : slots) {
            collision = collision || (slot.idPresent && slot.id == candidate);
        }
        if (!collision) {
            return candidate;
        }
    }
    return 0;
}

OperationResult readSshPrivateKeyRecord(
    std::uint64_t id,
    std::vector<std::uint8_t>& record)
{
    if (id == 0) {
        return {false, "SSH profile has no private-key record binding"};
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, true)) {
        return {false, "Failed to open SSH private-key records"};
    }
    SshPrivateKeyRecordSlots slots = {};
    OperationResult result = readSshPrivateKeyRecordSlots(preferences, slots);
    std::size_t slotIndex = slots.size();
    if (result.success) {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (slots[index].idPresent && slots[index].id == id) {
                slotIndex = index;
                break;
            }
        }
        if (slotIndex == slots.size() || !slots[slotIndex].blobPresent) {
            result = {false, "SSH private-key record is missing"};
        }
    }
    std::vector<std::uint8_t> loaded;
    if (result.success) {
        loaded.resize(slots[slotIndex].blobBytes);
        const String blobKey = sshPrivateKeyRecordBlobKey(slotIndex);
        if (preferences.getBytes(blobKey.c_str(), loaded.data(), loaded.size()) !=
            loaded.size() || decodeSshPrivateKeyId(loaded.data()) != id ||
            !sshPrivateKeyPemIsValid(
                loaded.data() + kSshPrivateKeyRecordHeaderBytes,
                loaded.size() - kSshPrivateKeyRecordHeaderBytes)) {
            result = {false, "SSH private-key record failed identity or PEM validation"};
        }
    }
    preferences.end();
    if (!result.success) {
        clearSshSecretBytes(loaded);
        return result;
    }
    record = std::move(loaded);
    return {true, ""};
}

OperationResult createSshPrivateKeyRecord(
    std::vector<std::uint8_t>& record,
    std::uint64_t& id)
{
    const OperationResult cleaned = cleanupSshPrivateKeyRecords();
    if (!cleaned.success) {
        return cleaned;
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, true)) {
        return {false, "Failed to inspect SSH private-key record capacity"};
    }
    SshPrivateKeyRecordSlots slots = {};
    OperationResult result = readSshPrivateKeyRecordSlots(preferences, slots);
    preferences.end();
    if (!result.success) {
        return result;
    }
    std::size_t freeSlot = slots.size();
    for (std::size_t index = 0; index < slots.size(); ++index) {
        if (!slots[index].idPresent && !slots[index].blobPresent) {
            freeSlot = index;
            break;
        }
    }
    if (freeSlot == slots.size()) {
        return {false, "All SSH private-key record slots are in use"};
    }
    const std::uint64_t generatedId = generateUniqueSshPrivateKeyId(slots);
    if (generatedId == 0) {
        return {false, "Failed to generate a unique SSH private-key record ID"};
    }
    encodeSshPrivateKeyId(generatedId, record.data());
    std::array<std::uint8_t, 32> expectedDigest = {};
    if (mbedtls_sha256(
            record.data(), record.size(), expectedDigest.data(), 0) != 0) {
        mbedtls_platform_zeroize(expectedDigest.data(), expectedDigest.size());
        return {false, "Failed to hash the SSH private-key record before writing"};
    }
    const String blobKey = sshPrivateKeyRecordBlobKey(freeSlot);
    if (!preferences.begin(kSshNamespace, false)) {
        mbedtls_platform_zeroize(expectedDigest.data(), expectedDigest.size());
        return {false, "Failed to open SSH private-key records for writing"};
    }
    preferences.putBytes(blobKey.c_str(), record.data(), record.size());
    preferences.end();

    const std::size_t recordBytes = record.size();
    mbedtls_platform_zeroize(record.data(), recordBytes);
    std::array<std::uint8_t, 32> verifiedDigest = {};
    bool readbackHashFailed = false;
    bool blobMatches = false;
    if (preferences.begin(kSshNamespace, true)) {
        if (preferences.getType(blobKey.c_str()) == PT_BLOB &&
            preferences.getBytesLength(blobKey.c_str()) == recordBytes &&
            preferences.getBytes(blobKey.c_str(), record.data(), recordBytes) ==
                recordBytes) {
            readbackHashFailed = mbedtls_sha256(
                record.data(), recordBytes, verifiedDigest.data(), 0) != 0;
            blobMatches = !readbackHashFailed &&
                decodeSshPrivateKeyId(record.data()) == generatedId &&
                std::equal(expectedDigest.begin(), expectedDigest.end(),
                           verifiedDigest.begin());
        }
        preferences.end();
    }
    mbedtls_platform_zeroize(expectedDigest.data(), expectedDigest.size());
    mbedtls_platform_zeroize(verifiedDigest.data(), verifiedDigest.size());
    if (readbackHashFailed) {
        cleanupSshPrivateKeyRecords();
        return {false, "Failed to hash the SSH private-key record after writing"};
    }
    if (!blobMatches) {
        cleanupSshPrivateKeyRecords();
        return {false, "NVS capacity could not store this SSH private key; delete an unused SSH profile/key or use a smaller key"};
    }

    const String idKey = sshPrivateKeyRecordIdKey(freeSlot);
    bool idMatches = false;
    if (preferences.begin(kSshNamespace, false)) {
        preferences.putULong64(idKey.c_str(), generatedId);
        idMatches = preferences.getType(idKey.c_str()) == PT_U64 &&
            preferences.getULong64(idKey.c_str(), 0) == generatedId;
        preferences.end();
    }
    if (!idMatches) {
        cleanupSshPrivateKeyRecords();
        return {false, "Failed to commit SSH private-key record identity"};
    }
    id = generatedId;
    return {true, ""};
}

enum class SshPrivateKeyBindingOutcome {
    Committed,
    Unchanged,
    Unknown,
};

struct SshPrivateKeyBindingResult {
    SshPrivateKeyBindingOutcome outcome;
    String error;
};

SshPrivateKeyBindingResult bindSshPrivateKeyToProfile(
    std::uint64_t profileId,
    std::uint64_t privateKeyId)
{
    std::vector<SshProfileSummary> profiles;
    std::size_t selected = 0;
    const OperationResult loaded = loadSshProfileSummaries(profiles, selected);
    if (!loaded.success) {
        return {SshPrivateKeyBindingOutcome::Unchanged, loaded.error};
    }
    std::size_t target = profiles.size();
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].id == profileId) {
            target = index;
            break;
        }
    }
    if (target == profiles.size()) {
        return {SshPrivateKeyBindingOutcome::Unchanged,
                "SSH profile changed before private-key binding"};
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {SshPrivateKeyBindingOutcome::Unchanged,
                "Failed to open SSH profile key binding for writing"};
    }
    std::vector<std::uint64_t> oldBindings;
    const OperationResult authority = readSshProfilePrivateKeyIds(
        preferences, oldBindings);
    const String profileIdKey = indexedProfileIdKey(target);
    const bool profileMatches = authority.success && target < oldBindings.size() &&
        preferences.getType(profileIdKey.c_str()) == PT_U64 &&
        preferences.getULong64(profileIdKey.c_str(), 0) == profileId;
    if (!profileMatches) {
        preferences.end();
        return {SshPrivateKeyBindingOutcome::Unchanged,
                authority.success
                    ? String("SSH profile changed before private-key binding")
                    : authority.error};
    }
    const std::uint64_t oldBinding = oldBindings[target];
    if (oldBinding == privateKeyId) {
        preferences.end();
        return {SshPrivateKeyBindingOutcome::Committed, ""};
    }
    const String key = indexedPrivateKeyIdKey(target);
    preferences.putULong64(key.c_str(), privateKeyId);
    std::vector<std::uint64_t> committedBindings;
    const OperationResult committedAuthority = readSshProfilePrivateKeyIds(
        preferences, committedBindings);
    const bool committedProfileMatches = committedAuthority.success &&
        target < committedBindings.size() &&
        preferences.getType(profileIdKey.c_str()) == PT_U64 &&
        preferences.getULong64(profileIdKey.c_str(), 0) == profileId;
    const bool committed = committedProfileMatches &&
        committedBindings[target] == privateKeyId;
    const bool unchanged = committedProfileMatches &&
        committedBindings[target] == oldBinding;
    preferences.end();
    if (committed) {
        return {SshPrivateKeyBindingOutcome::Committed, ""};
    }
    if (unchanged) {
        return {SshPrivateKeyBindingOutcome::Unchanged,
                "Failed to commit SSH profile private-key binding"};
    }
    return {SshPrivateKeyBindingOutcome::Unknown,
            "SSH profile private-key binding outcome is unknown; reload the profile before retrying"};
}

OperationResult loadLegacySshPrivateKeyOwner(std::uint64_t& id, bool& found)
{
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, true)) {
        return {false, "Failed to open legacy SSH private-key owner"};
    }
    const OperationResult result = readOptionalSshPrivateKeyId(
        preferences, kLegacySshPrivateKeyOwnerKey, id, found);
    preferences.end();
    return result;
}

OperationResult storeLegacySshPrivateKeyOwner(std::uint64_t id)
{
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open legacy SSH private-key owner for writing"};
    }
    preferences.putULong64(kLegacySshPrivateKeyOwnerKey, id);
    const bool matches = preferences.getType(kLegacySshPrivateKeyOwnerKey) == PT_U64 &&
        preferences.getULong64(kLegacySshPrivateKeyOwnerKey, 0) == id;
    preferences.end();
    return matches
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to preserve the unassigned legacy SSH key record"};
}

OperationResult clearLegacySshPrivateKeyOwner()
{
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open legacy SSH private-key owner for cleanup"};
    }
    const bool removed = removeExactSshPreference(
        preferences, kLegacySshPrivateKeyOwnerKey);
    preferences.end();
    return removed
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to clear the legacy SSH private-key owner"};
}

OperationResult migrateLegacySshPrivateKey()
{
    if (!SD.exists(kLegacySshPrivateKeyPath)) {
        return {true, ""};
    }
    std::vector<std::uint8_t> legacyRecord;
    OperationResult result = readSshPrivateKeyFile(
        kLegacySshPrivateKeyPath, legacyRecord);
    if (!result.success) {
        return result;
    }
    std::vector<SshProfileSummary> profiles;
    std::size_t selected = 0;
    result = loadSshProfileSummaries(profiles, selected);
    std::vector<std::uint64_t> profileKeyIds;
    if (result.success) {
        Preferences preferences;
        if (!preferences.begin(kSshNamespace, true)) {
            result = {false, "Failed to inspect legacy SSH profile key bindings"};
        } else {
            result = readSshProfilePrivateKeyIds(preferences, profileKeyIds);
            preferences.end();
        }
    }
    std::uint64_t ownerId = 0;
    bool ownerFound = false;
    if (result.success) {
        result = loadLegacySshPrivateKeyOwner(ownerId, ownerFound);
    }
    std::uint64_t recordId = ownerFound ? ownerId : 0;
    if (result.success) {
        for (const std::uint64_t id : profileKeyIds) {
            if (id == 0) {
                continue;
            }
            if (recordId != 0 && recordId != id) {
                result = {false, "Legacy SSH key migration found conflicting profile bindings"};
                break;
            }
            recordId = id;
        }
    }
    if (result.success && recordId != 0) {
        std::vector<std::uint8_t> storedRecord;
        result = readSshPrivateKeyRecord(recordId, storedRecord);
        if (result.success &&
            (storedRecord.size() != legacyRecord.size() ||
             !std::equal(storedRecord.begin() + kSshPrivateKeyRecordHeaderBytes,
                         storedRecord.end(),
                         legacyRecord.begin() + kSshPrivateKeyRecordHeaderBytes))) {
            result = {false, "Legacy SSH key does not match its committed NVS record"};
        }
        clearSshSecretBytes(storedRecord);
    }
    if (result.success && recordId == 0) {
        result = createSshPrivateKeyRecord(legacyRecord, recordId);
    }
    if (result.success && profiles.empty()) {
        result = storeLegacySshPrivateKeyOwner(recordId);
    }
    if (result.success && !profiles.empty()) {
        for (std::size_t index = 0; index < profiles.size(); ++index) {
            if (profileKeyIds[index] == 0) {
                const SshPrivateKeyBindingResult binding =
                    bindSshPrivateKeyToProfile(profiles[index].id, recordId);
                result = binding.outcome == SshPrivateKeyBindingOutcome::Committed
                    ? OperationResult{true, ""}
                    : OperationResult{false, binding.error};
            } else if (profileKeyIds[index] != recordId) {
                result = {false, "Legacy SSH key migration found a conflicting profile binding"};
            }
            if (!result.success) {
                break;
            }
        }
        if (result.success && ownerFound) {
            result = clearLegacySshPrivateKeyOwner();
        }
    }
    clearSshSecretBytes(legacyRecord);
    if (!result.success) {
        return result;
    }
    if (!SD.remove(kLegacySshPrivateKeyPath)) {
        return {false, "SSH key record and bindings committed, but legacy microSD key cleanup failed"};
    }
    return {true, ""};
}

SshProfileSummary readIndexedProfileSummary(Preferences& preferences,
                                            std::size_t index)
{
    const String suffix(index);
    return {
        preferences.getULong64(indexedProfileIdKey(index).c_str(), 0),
        preferences.getString((String("n") + suffix).c_str(), ""),
        preferences.getString((String("h") + suffix).c_str(), ""),
        preferences.getUShort((String("p") + suffix).c_str(), 22),
        preferences.getString((String("u") + suffix).c_str(), ""),
        preferences.getUChar((String("a") + suffix).c_str(), 0) == 1
            ? SshAuthMode::PrivateKey : SshAuthMode::Password,
    };
}

std::uint64_t generateUniqueSshProfileId(
    const std::vector<SshProfileSummary>& profiles)
{
    for (std::size_t attempt = 0; attempt < 32; ++attempt) {
        const std::uint64_t candidate =
            (static_cast<std::uint64_t>(esp_random()) << 32) |
            static_cast<std::uint64_t>(esp_random());
        if (candidate == 0) {
            continue;
        }
        bool collision = false;
        for (const SshProfileSummary& profile : profiles) {
            if (profile.id == candidate) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            return candidate;
        }
    }
    return 0;
}

OperationResult assignMissingSshProfileIds(
    Preferences& preferences,
    std::vector<SshProfileSummary>& profiles)
{
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].id == 0) {
            continue;
        }
        for (std::size_t other = index + 1; other < profiles.size(); ++other) {
            if (profiles[index].id == profiles[other].id) {
                return {false, "Stored SSH profile IDs are not unique"};
            }
        }
    }
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].id != 0) {
            continue;
        }
        const std::uint64_t id = generateUniqueSshProfileId(profiles);
        if (id == 0) {
            return {false, "Failed to generate a unique SSH profile ID"};
        }
        const String key = indexedProfileIdKey(index);
        if (preferences.putULong64(key.c_str(), id) != sizeof(std::uint64_t) ||
            preferences.getULong64(key.c_str(), 0) != id) {
            return {false, "Failed to persist SSH profile ID " + String(index + 1)};
        }
        profiles[index].id = id;
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
        preferences.getULong64(indexedPrivateKeyIdKey(index).c_str(), 0),
    };
}

bool writeIndexedProfile(Preferences& preferences, const SshProfile& profile,
                         std::size_t index)
{
    const String suffix(index);
    bool written = preferences.putString((String("n") + suffix).c_str(), profile.name) == profile.name.length() &&
           preferences.putString((String("h") + suffix).c_str(), profile.host) == profile.host.length() &&
           preferences.putUShort((String("p") + suffix).c_str(), profile.port) == sizeof(std::uint16_t) &&
           preferences.putString((String("u") + suffix).c_str(), profile.username) == profile.username.length() &&
           preferences.putString((String("w") + suffix).c_str(), profile.password) == profile.password.length() &&
           preferences.putUChar((String("a") + suffix).c_str(),
                                profile.authMode == SshAuthMode::PrivateKey ? 1 : 0) == sizeof(std::uint8_t) &&
           preferences.putString((String("k") + suffix).c_str(), profile.privateKeyPassphrase) ==
               profile.privateKeyPassphrase.length();
    const String privateKeyIdKey = indexedPrivateKeyIdKey(index);
    if (profile.privateKeyId == 0) {
        written = removeExactSshPreference(preferences, privateKeyIdKey) && written;
    } else {
        written = preferences.putULong64(privateKeyIdKey.c_str(), profile.privateKeyId) ==
                      sizeof(profile.privateKeyId) && written;
    }
    return written;
}

bool removeIndexedProfile(Preferences& preferences, std::size_t index)
{
    const String suffix(index);
    if (!removeExactSshPreference(preferences, String("n") + suffix) ||
        !removeExactSshPreference(preferences, String("h") + suffix) ||
        !removeExactSshPreference(preferences, String("p") + suffix) ||
        !removeExactSshPreference(preferences, String("u") + suffix) ||
        !removeExactSshPreference(preferences, String("w") + suffix) ||
        !removeExactSshPreference(preferences, String("a") + suffix) ||
        !removeExactSshPreference(preferences, String("k") + suffix) ||
        !removeExactSshPreference(preferences, indexedProfileIdKey(index)) ||
        !removeExactSshPreference(preferences, indexedPrivateKeyIdKey(index))) {
        return false;
    }
    return true;
}

OperationResult verifyWrittenSshProfiles(
    const std::vector<SshProfile>& profiles,
    std::size_t selectedIndex,
    const std::vector<std::uint64_t>& profileIds)
{
    std::vector<SshProfile> verified;
    std::size_t verifiedSelected = 0;
    const OperationResult loaded = loadSshProfiles(verified, verifiedSelected);
    if (!loaded.success || verified.size() != profiles.size() ||
        (!profiles.empty() && verifiedSelected != selectedIndex)) {
        return {false, "Failed to verify SSH profiles after NVS write"};
    }
    std::vector<SshProfileSummary> verifiedSummaries;
    std::size_t verifiedSummarySelected = 0;
    const OperationResult summariesLoaded = loadSshProfileSummaries(
        verifiedSummaries, verifiedSummarySelected);
    if (!summariesLoaded.success || verifiedSummaries.size() != profileIds.size() ||
        (!verifiedSummaries.empty() && verifiedSummarySelected != selectedIndex)) {
        return {false, "Failed to verify SSH profile IDs after NVS write"};
    }
    for (std::size_t index = 0; index < profileIds.size(); ++index) {
        if (verifiedSummaries[index].id != profileIds[index] ||
            verified[index].privateKeyId != profiles[index].privateKeyId) {
            return {false, "Failed to verify SSH profile IDs after NVS write"};
        }
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, true)) {
        return {false, "Failed to open SSH settings in NVS for verification"};
    }
    bool inactiveIdentityAbsent = true;
    for (std::size_t index = profiles.size(); index < kMaximumSshProfiles; ++index) {
        if (preferences.getType(indexedProfileIdKey(index).c_str()) != PT_INVALID ||
            preferences.getType(indexedPrivateKeyIdKey(index).c_str()) != PT_INVALID) {
            inactiveIdentityAbsent = false;
            break;
        }
    }
    preferences.end();
    return inactiveIdentityAbsent
        ? OperationResult{true, ""}
        : OperationResult{false, "Inactive SSH profile identity remains in NVS"};
}

OperationResult writeProfiles(const std::vector<SshProfile>& profiles,
                              std::size_t selectedIndex,
                              const std::vector<std::uint64_t>& profileIds)
{
    if (profiles.size() > kMaximumSshProfiles ||
        profiles.size() != profileIds.size() ||
        (!profiles.empty() && selectedIndex >= profiles.size())) {
        return {false, "SSH profile collection is invalid"};
    }
    for (std::size_t index = 0; index < profileIds.size(); ++index) {
        if (profileIds[index] == 0) {
            return {false, "SSH profile collection contains an invalid ID"};
        }
        for (std::size_t other = index + 1; other < profileIds.size(); ++other) {
            if (profileIds[index] == profileIds[other]) {
                return {false, "SSH profile collection contains duplicate IDs"};
            }
        }
    }
    const OperationResult cleaned = cleanupSshPrivateKeyRecords();
    if (!cleaned.success) {
        return cleaned;
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH settings in NVS for writing"};
    }
    bool written = true;
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        written = writeIndexedProfile(preferences, profiles[index], index) && written;
        written = preferences.putULong64(indexedProfileIdKey(index).c_str(),
                                         profileIds[index]) ==
                      sizeof(std::uint64_t) && written;
    }
    for (std::size_t index = profiles.size(); index < kMaximumSshProfiles; ++index) {
        written = removeIndexedProfile(preferences, index) && written;
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
    return verifyWrittenSshProfiles(profiles, selectedIndex, profileIds);
}

OperationResult writeProfilesAfterIndexedDelete(
    const std::vector<SshProfile>& profiles,
    std::size_t selectedIndex,
    const std::vector<std::uint64_t>& profileIds,
    std::size_t deletedIndex)
{
    if (profiles.size() >= kMaximumSshProfiles ||
        profiles.size() != profileIds.size() ||
        deletedIndex > profiles.size() ||
        (!profiles.empty() && selectedIndex >= profiles.size())) {
        return {false, "SSH profile deletion state is invalid"};
    }
    Preferences authority;
    if (!authority.begin(kSshNamespace, true)) {
        return {false, "Failed to open SSH settings in NVS for deletion"};
    }
    const PreferenceType countType = authority.getType("cnt");
    authority.end();
    if (countType == PT_INVALID) {
        if (!profiles.empty() || deletedIndex != 0) {
            return {false, "Legacy SSH profile deletion state is invalid"};
        }
        return writeProfiles(profiles, selectedIndex, profileIds);
    }
    if (countType != PT_U8) {
        return {false, "Stored SSH profile count has an invalid type"};
    }
    const OperationResult cleaned = cleanupSshPrivateKeyRecords();
    if (!cleaned.success) {
        return cleaned;
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH settings in NVS for deletion"};
    }
    const std::size_t oldCount = profiles.size() + 1;
    if (preferences.getType("cnt") != PT_U8 ||
        preferences.getUChar("cnt", 0) != oldCount) {
        preferences.end();
        return {false, "Stored SSH profile count changed before deletion"};
    }
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        const std::size_t storedIndex = index < deletedIndex ? index : index + 1;
        const String storedIdKey = indexedProfileIdKey(storedIndex);
        if (profileIds[index] == 0 ||
            preferences.getType(storedIdKey.c_str()) != PT_U64 ||
            preferences.getULong64(storedIdKey.c_str(), 0) != profileIds[index]) {
            preferences.end();
            return {false, "Stored SSH profile IDs changed before deletion"};
        }
    }
    for (std::size_t index = deletedIndex; index < profiles.size(); ++index) {
        const String destinationIdKey = indexedProfileIdKey(index);
        const String sourceIdKey = indexedProfileIdKey(index + 1);
        const std::uint64_t shiftedId = profileIds[index];
        preferences.putULong64(destinationIdKey.c_str(), shiftedId);
        if (preferences.getType(destinationIdKey.c_str()) != PT_U64 ||
            preferences.getULong64(destinationIdKey.c_str(), 0) != shiftedId ||
            preferences.getType(sourceIdKey.c_str()) != PT_U64 ||
            preferences.getULong64(sourceIdKey.c_str(), 0) != shiftedId ||
            !writeIndexedProfile(preferences, profiles[index], index)) {
            preferences.end();
            return {false, "Failed to shift SSH profile safely during deletion"};
        }
    }
    for (std::size_t index = profiles.size(); index < kMaximumSshProfiles; ++index) {
        if (!removeIndexedProfile(preferences, index)) {
            preferences.end();
            return {false, "Failed to remove inactive SSH profile state"};
        }
    }
    const std::uint8_t storedSelection = profiles.empty()
        ? 0
        : static_cast<std::uint8_t>(selectedIndex);
    preferences.putUChar("sel", storedSelection);
    if (preferences.getUChar("sel", 0) != storedSelection) {
        preferences.end();
        return {false, "Failed to persist selected SSH profile during deletion"};
    }
    preferences.putUChar("cnt", static_cast<std::uint8_t>(profiles.size()));
    if (preferences.getUChar("cnt", static_cast<std::uint8_t>(oldCount)) != profiles.size()) {
        preferences.end();
        return {false, "Failed to commit SSH profile deletion"};
    }
    preferences.end();
    return verifyWrittenSshProfiles(profiles, selectedIndex, profileIds);
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

OperationResult loadSshProfileSummaries(
    std::vector<SshProfileSummary>& profiles,
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
    std::vector<SshProfileSummary> loaded;
    loaded.reserve(count == 0 ? 1 : count);
    std::size_t selected = 0;
    if (count == 0) {
        const String host = preferences.getString("host", "");
        if (!host.isEmpty()) {
            loaded.push_back({
                preferences.getULong64(indexedProfileIdKey(0).c_str(), 0),
                host.substring(0, 32),
                host,
                preferences.getUShort("port", 22),
                preferences.getString("user", ""),
                preferences.getUChar("auth_mode", 0) == 1
                    ? SshAuthMode::PrivateKey : SshAuthMode::Password,
            });
        }
    } else {
        selected = preferences.getUChar("sel", 0);
        if (selected >= count) {
            preferences.end();
            return {false, "Stored selected SSH profile index is invalid"};
        }
        for (std::size_t index = 0; index < count; ++index) {
            loaded.push_back(readIndexedProfileSummary(preferences, index));
        }
    }
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        const SshProfileSummary& item = loaded[index];
        if (!isValidSshProfileName(item.name) || !isValidSshHost(item.host) ||
            item.port == 0 || !isValidSshUsername(item.username)) {
            preferences.end();
            return {false, "Stored SSH profile " + String(index + 1) + " is invalid"};
        }
    }
    const OperationResult assigned = assignMissingSshProfileIds(preferences, loaded);
    preferences.end();
    if (!assigned.success) {
        return assigned;
    }
    profiles = std::move(loaded);
    selectedIndex = selected;
    return {true, ""};
}

OperationResult loadSshProfile(SshProfile& profile)
{
    std::vector<SshProfileSummary> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult result = loadSshProfileSummaries(profiles, selectedIndex);
    if (!result.success) {
        return result;
    }
    if (profiles.empty()) {
        profile = {"", "", 22, "", "", SshAuthMode::Password, ""};
        return {true, ""};
    }
    const SshProfileSummary& selected = profiles[selectedIndex];
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, true)) {
        return {false, "Failed to open SSH secrets in NVS"};
    }
    const std::size_t count = preferences.getUChar("cnt", 0);
    String password;
    String privateKeyPassphrase;
    std::uint64_t privateKeyId = 0;
    bool privateKeyIdFound = false;
    if (count == 0) {
        if (profiles.size() != 1 || selectedIndex != 0) {
            preferences.end();
            return {false, "Stored legacy SSH profile selection changed during load"};
        }
        password = preferences.getString("password", "");
        privateKeyPassphrase = preferences.getString("key_pass", "");
        const OperationResult keyResult = readOptionalSshPrivateKeyId(
            preferences, indexedPrivateKeyIdKey(0),
            privateKeyId, privateKeyIdFound);
        if (!keyResult.success) {
            preferences.end();
            return keyResult;
        }
    } else {
        const std::size_t storedSelected = preferences.getUChar("sel", 0);
        if (count != profiles.size() || storedSelected != selectedIndex) {
            preferences.end();
            return {false, "Stored SSH profile selection changed during load"};
        }
        const String suffix(selectedIndex);
        password = preferences.getString((String("w") + suffix).c_str(), "");
        privateKeyPassphrase = preferences.getString(
            (String("k") + suffix).c_str(), "");
        const OperationResult keyResult = readOptionalSshPrivateKeyId(
            preferences, indexedPrivateKeyIdKey(selectedIndex),
            privateKeyId, privateKeyIdFound);
        if (!keyResult.success) {
            preferences.end();
            return keyResult;
        }
    }
    preferences.end();
    profile = {
        selected.name,
        selected.host,
        selected.port,
        selected.username,
        password,
        selected.authMode,
        privateKeyPassphrase,
        privateKeyIdFound ? privateKeyId : 0,
    };
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
        preferences.getULong64(indexedPrivateKeyIdKey(0).c_str(), 0),
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
    if (index == profiles.size() && profiles.size() >= kMaximumSshProfiles) {
        return {false, "SSH profile limit of five has been reached"};
    }
    if (index > profiles.size() || index >= kMaximumSshProfiles) {
        return {false, "SSH profile index is outside the supported range"};
    }
    std::vector<SshProfileSummary> summaries;
    std::size_t summarySelectedIndex = 0;
    const OperationResult summariesLoaded = loadSshProfileSummaries(
        summaries, summarySelectedIndex);
    if (!summariesLoaded.success) {
        return summariesLoaded;
    }
    if (summaries.size() != profiles.size() ||
        (!summaries.empty() && summarySelectedIndex != selectedIndex)) {
        return {false, "SSH profile summary does not match stored profiles"};
    }
    std::vector<std::uint64_t> profileIds;
    profileIds.reserve(summaries.size() + 1);
    for (const SshProfileSummary& summary : summaries) {
        profileIds.push_back(summary.id);
    }
    std::uint64_t legacyOwnerId = 0;
    bool legacyOwnerFound = false;
    if (index == profiles.size() && profiles.empty() && profile.privateKeyId == 0) {
        const OperationResult owner = loadLegacySshPrivateKeyOwner(
            legacyOwnerId, legacyOwnerFound);
        if (!owner.success) {
            return owner;
        }
    }
    if (index == profiles.size()) {
        const std::uint64_t id = generateUniqueSshProfileId(summaries);
        if (id == 0) {
            return {false, "Failed to generate a unique SSH profile ID"};
        }
        SshProfile created = profile;
        if (legacyOwnerFound) {
            created.privateKeyId = legacyOwnerId;
        }
        profiles.push_back(created);
        profileIds.push_back(id);
        selectedIndex = index;
    } else {
        profiles[index] = profile;
    }
    const OperationResult written = writeProfiles(profiles, selectedIndex, profileIds);
    if (written.success && legacyOwnerFound) {
        clearLegacySshPrivateKeyOwner();
    }
    return written;
}

OperationResult selectSshProfile(std::size_t index)
{
    std::vector<SshProfileSummary> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfileSummaries(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    if (index >= profiles.size()) {
        return {false, "SSH profile selection is outside the stored range"};
    }
    if (index == selectedIndex) {
        return {true, ""};
    }
    Preferences preferences;
    if (!preferences.begin(kSshNamespace, false)) {
        return {false, "Failed to open SSH settings in NVS for selection"};
    }
    const bool written = preferences.getUChar("cnt", 0) == profiles.size() &&
        preferences.putUChar("sel", static_cast<std::uint8_t>(index)) ==
            sizeof(std::uint8_t) &&
        preferences.getUChar("sel", 0) == index;
    preferences.end();
    return written
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to persist selected SSH profile"};
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
    std::vector<SshProfileSummary> summaries;
    std::size_t summarySelectedIndex = 0;
    const OperationResult summariesLoaded = loadSshProfileSummaries(
        summaries, summarySelectedIndex);
    if (!summariesLoaded.success) {
        return summariesLoaded;
    }
    if (summaries.size() != profiles.size() ||
        (!summaries.empty() && summarySelectedIndex != selectedIndex)) {
        return {false, "SSH profile summary does not match stored profiles"};
    }
    std::vector<std::uint64_t> profileIds;
    profileIds.reserve(summaries.size());
    for (const SshProfileSummary& summary : summaries) {
        profileIds.push_back(summary.id);
    }
    profiles.erase(profiles.begin() + static_cast<std::ptrdiff_t>(index));
    profileIds.erase(profileIds.begin() + static_cast<std::ptrdiff_t>(index));
    if (profiles.empty()) {
        selectedIndex = 0;
    } else if (selectedIndex > index) {
        --selectedIndex;
    } else if (selectedIndex >= profiles.size()) {
        selectedIndex = profiles.size() - 1;
    }
    return writeProfilesAfterIndexedDelete(
        profiles, selectedIndex, profileIds, index);
}

bool sshProfileIsComplete(const SshProfile& profile)
{
    return isValidSshHost(profile.host) && profile.port != 0 &&
           isValidSshUsername(profile.username) &&
           ((profile.authMode == SshAuthMode::Password && !profile.password.isEmpty()) ||
            (profile.authMode == SshAuthMode::PrivateKey &&
             sshPrivateKeyIsInstalled(profile.privateKeyId)));
}

OperationResult initializeSshStorage()
{
    const OperationResult directory = ensureSshDirectory();
    if (!directory.success) {
        return directory;
    }
    const OperationResult recovered = recoverAtomicSdFile(kSshKnownHostsPath);
    if (!recovered.success) {
        return recovered;
    }
    std::vector<SshProfileSummary> profiles;
    std::size_t selectedIndex = 0;
    const OperationResult loaded = loadSshProfileSummaries(profiles, selectedIndex);
    if (!loaded.success) {
        return loaded;
    }
    const OperationResult cleaned = cleanupSshPrivateKeyRecords();
    if (!cleaned.success) {
        return cleaned;
    }
    return migrateLegacySshPrivateKey();
}

OperationResult installSshPrivateKey(const String& temporaryPath,
                                     std::uint64_t profileId)
{
    if (profileId == 0) {
        return {false, "Select an SSH profile before installing its private key"};
    }
    std::vector<SshProfileSummary> profiles;
    std::size_t selected = 0;
    OperationResult result = loadSshProfileSummaries(profiles, selected);
    bool profileFound = false;
    if (result.success) {
        for (const SshProfileSummary& profile : profiles) {
            profileFound = profileFound || profile.id == profileId;
        }
        if (!profileFound) {
            result = {false, "Selected SSH profile no longer exists"};
        }
    }
    std::vector<std::uint8_t> record;
    if (result.success) {
        result = readSshPrivateKeyFile(temporaryPath, record);
    }
    std::uint64_t privateKeyId = 0;
    if (result.success) {
        result = createSshPrivateKeyRecord(record, privateKeyId);
    }
    if (result.success) {
        const SshPrivateKeyBindingResult binding =
            bindSshPrivateKeyToProfile(profileId, privateKeyId);
        if (binding.outcome == SshPrivateKeyBindingOutcome::Committed) {
            result = {true, ""};
        } else {
            result = {false, binding.error};
            if (binding.outcome == SshPrivateKeyBindingOutcome::Unchanged) {
                const OperationResult cleaned = cleanupSshPrivateKeyRecords();
                if (!cleaned.success) {
                    result.error += "; unbound key cleanup also failed: " + cleaned.error;
                }
            }
        }
    }
    clearSshSecretBytes(record);
    return result;
}

bool sshPrivateKeyIsInstalled(std::uint64_t privateKeyId)
{
    std::vector<std::uint8_t> record;
    const OperationResult result = readSshPrivateKeyRecord(privateKeyId, record);
    clearSshSecretBytes(record);
    return result.success;
}

OperationResult loadTrustedSshFingerprint(const String& host,
                                          std::uint16_t port,
                                          String& fingerprint,
                                          bool& found)
{
    fingerprint = "";
    found = false;
    if (!isValidSshHost(host) || port == 0) {
        return {false, "SSH trust lookup received invalid host data"};
    }
    File file = SD.open(kSshKnownHostsPath, FILE_READ);
    if (!file) {
        return {true, ""};
    }
    if (file.isDirectory() || file.size() > kMaximumKnownHostsBytes) {
        file.close();
        return {false, "SSH known-hosts file is invalid or too large"};
    }
    const String prefix = knownHostPrefix(host, port);
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith(prefix)) {
            fingerprint = line.substring(prefix.length());
            file.close();
            if (!isValidSshFingerprint(fingerprint)) {
                fingerprint = "";
                return {false, "Stored SSH host fingerprint is invalid"};
            }
            found = true;
            return {true, ""};
        }
    }
    file.close();
    return {true, ""};
}

namespace {

template <typename Attempt>
int runUntilCompleteControlled(
    Attempt attempt,
    std::uint32_t timeoutMs,
    const std::function<bool()>& isCancelled,
    bool& cancelled)
{
    const std::uint32_t deadline = millis() + timeoutMs;
    int result = LIBSSH2_ERROR_EAGAIN;
    while (result == LIBSSH2_ERROR_EAGAIN &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        if (isCancelled()) {
            cancelled = true;
            return LIBSSH2_ERROR_TIMEOUT;
        }
        result = attempt();
        delay(5);
    }
    return result == LIBSSH2_ERROR_EAGAIN ? LIBSSH2_ERROR_TIMEOUT : result;
}

}  // namespace

SshTrustResult checkTrustedSshHost(const String& host, std::uint16_t port,
                                   const String& fingerprint)
{
    if (!isValidSshFingerprint(fingerprint)) {
        return {false, false, false, "", "SSH trust lookup received invalid host data"};
    }
    String trusted;
    bool found = false;
    const OperationResult loaded = loadTrustedSshFingerprint(
        host, port, trusted, found);
    if (!loaded.success) {
        return {false, found, false, "", loaded.error};
    }
    return {true, found, found && trusted == fingerprint, trusted, ""};
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
    const OperationResult recovered = recoverAtomicSdFile(kSshKnownHostsPath);
    if (!recovered.success) {
        return recovered;
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
    return commitStagedSdFile(kSshKnownHostsPath, kSshKnownHostsTemporaryPath);
}

OperationResult forgetTrustedSshHost(const String& host, std::uint16_t port)
{
    if (!isValidSshHost(host) || port == 0) {
        return {false, "Cannot forget an invalid SSH host"};
    }
    const OperationResult recovered = recoverAtomicSdFile(kSshKnownHostsPath);
    if (!recovered.success) {
        return recovered;
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
    return commitStagedSdFile(kSshKnownHostsPath, kSshKnownHostsTemporaryPath);
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
    const std::function<bool()> isCancelled = []() { return false; };
    return connectControlled(profile, timeoutMs, isCancelled);
}

OperationResult SshClient::connectControlled(
    const SshProfile& profile,
    std::uint32_t timeoutMs,
    const std::function<bool()>& isCancelled)
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
    if (!isCancelled) {
        return {false, "SSH connection requires a cancellation callback"};
    }
    if (isCancelled()) {
        return {false, "SSH connection canceled by user"};
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
    if (isCancelled()) {
        close();
        return {false, "SSH connection canceled by user"};
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
    bool cancelled = false;
    const int handshake = runUntilCompleteControlled(
        [this]() {
            return libssh2_session_handshake(
                implementation_->session, implementation_->network.fd());
        }, timeoutMs, isCancelled, cancelled);
    if (cancelled) {
        close();
        return {false, "SSH connection canceled by user"};
    }
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
    const std::function<bool()> isCancelled = []() { return false; };
    return authenticateControlled(profile, timeoutMs, isCancelled);
}

OperationResult SshClient::authenticateControlled(
    const SshProfile& profile,
    std::uint32_t timeoutMs,
    const std::function<bool()>& isCancelled)
{
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    if (!isCancelled) {
        return {false, "SSH authentication requires a cancellation callback"};
    }
    if (isCancelled()) {
        close();
        return {false, "SSH authentication canceled by user"};
    }
    int authentication = 0;
    bool cancelled = false;
    if (profile.authMode == SshAuthMode::Password) {
        authentication = runUntilCompleteControlled(
            [this, &profile]() {
                return libssh2_userauth_password_ex(
                    implementation_->session, profile.username.c_str(),
                    static_cast<unsigned int>(profile.username.length()),
                    profile.password.c_str(),
                    static_cast<unsigned int>(profile.password.length()), nullptr);
            }, timeoutMs, isCancelled, cancelled);
    } else {
        const char* passphrase = profile.privateKeyPassphrase.isEmpty()
            ? nullptr : profile.privateKeyPassphrase.c_str();
        std::vector<std::uint8_t> privateKeyRecord;
        const OperationResult loaded = readSshPrivateKeyRecord(
            profile.privateKeyId, privateKeyRecord);
        if (!loaded.success) {
            close();
            return loaded;
        }
        const char* privateKey = reinterpret_cast<const char*>(
            privateKeyRecord.data() + kSshPrivateKeyRecordHeaderBytes);
        const std::size_t privateKeyBytes =
            privateKeyRecord.size() - kSshPrivateKeyRecordHeaderBytes;
        authentication = runUntilCompleteControlled(
            [this, &profile, passphrase, privateKey, privateKeyBytes]() {
                return libssh2_userauth_publickey_frommemory(
                    implementation_->session, profile.username.c_str(),
                    profile.username.length(), nullptr, 0,
                    privateKey, privateKeyBytes, passphrase);
            }, timeoutMs, isCancelled, cancelled);
        clearSshSecretBytes(privateKeyRecord);
    }
    if (cancelled) {
        close();
        return {false, "SSH authentication canceled by user"};
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
    const std::function<bool()> isCancelled = []() { return false; };
    return executeCommandControlled(
        command, output, exitStatus, maximumOutputBytes, timeoutMs,
        isCancelled);
}

OperationResult SshClient::executeCommandControlled(
    const String& command,
    std::string& output,
    int& exitStatus,
    std::size_t maximumOutputBytes,
    std::uint32_t timeoutMs,
    const std::function<bool()>& isCancelled)
{
    output.clear();
    if (maximumOutputBytes == 0 || maximumOutputBytes > 16384) {
        return {false, "SSH command output limit must be between 1 and 16384 bytes"};
    }
    const SshCommandOutputCallback onOutput =
        [&output, maximumOutputBytes](
            const std::uint8_t* data,
            std::size_t bytes) -> OperationResult {
        if (!appendSshCommandOutput(
                output, reinterpret_cast<const char*>(data), bytes,
                maximumOutputBytes)) {
            return {
                false,
                String("SSH command output exceeded the configured ") +
                    String(static_cast<unsigned long>(maximumOutputBytes)) +
                    "-byte inline limit",
            };
        }
        return {true, ""};
    };
    return executeCommandStreamingControlled(
        command, exitStatus, timeoutMs, isCancelled, onOutput);
}

OperationResult SshClient::executeCommandStreamingControlled(
    const String& command,
    int& exitStatus,
    std::uint32_t timeoutMs,
    const std::function<bool()>& isCancelled,
    const SshCommandOutputCallback& onOutput)
{
    exitStatus = -1;
    if (implementation_ == nullptr || implementation_->session == nullptr) {
        return {false, "SSH session is not connected"};
    }
    if (implementation_->channel != nullptr) {
        return {false, "SSH session already has an open channel"};
    }
    if (command.isEmpty() || command.length() > 1024) {
        return {false, "SSH command must contain 1 to 1024 bytes without NUL characters"};
    }
    if (timeoutMs < 1000 || timeoutMs > 120000) {
        return {false, "SSH command timeout must be between 1000 and 120000 ms"};
    }
    if (!isCancelled) {
        return {false, "SSH command requires a cancellation callback"};
    }
    if (!onOutput) {
        return {false, "SSH command requires an output callback"};
    }
    if (isCancelled()) {
        return {false, "SSH command canceled by user"};
    }
    const std::uint32_t deadline = millis() + timeoutMs;
    while (implementation_->channel == nullptr &&
           static_cast<std::int32_t>(deadline - millis()) > 0) {
        if (isCancelled()) {
            return {false, "SSH command canceled by user"};
        }
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
    bool cancelled = false;
    const int started = runUntilCompleteControlled(
        [this, &command]() {
            return libssh2_channel_exec(implementation_->channel, command.c_str());
        }, timeoutMs, isCancelled, cancelled);
    if (cancelled) {
        return {false, "SSH command canceled by user"};
    }
    if (started != 0) {
        return {false, sessionError(implementation_->session,
                                    "SSH command execution", started)};
    }
    std::uint8_t buffer[1024] = {};
    const std::uint32_t readDeadline = millis() + timeoutMs;
    while (static_cast<std::int32_t>(readDeadline - millis()) > 0) {
        if (isCancelled()) {
            return {false, "SSH command canceled by user"};
        }
        bool progressed = false;
        for (int streamId = 0; streamId <= 1; ++streamId) {
            const ssize_t bytes = libssh2_channel_read_ex(
                implementation_->channel, streamId,
                reinterpret_cast<char*>(buffer), sizeof(buffer));
            if (bytes > 0) {
                const std::size_t count = static_cast<std::size_t>(bytes);
                const OperationResult appended = onOutput(buffer, count);
                if (!appended.success) {
                    return appended;
                }
                progressed = true;
            } else if (bytes < 0 && bytes != LIBSSH2_ERROR_EAGAIN) {
                return {false, sessionError(implementation_->session,
                                            streamId == 0 ? "SSH command stdout read"
                                                          : "SSH command stderr read",
                                            static_cast<int>(bytes))};
            }
        }
        if (isCancelled()) {
            return {false, "SSH command canceled by user"};
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
    if (!isValidWorkspaceFilename(workspaceName.c_str())) {
        return {false, "SFTP download destination filename is invalid"};
    }
    const String localPath = workspaceFilePath(workspaceName);
    const OperationResult parent = ensureWorkspaceFileParent(workspaceName);
    if (!parent.success) {
        return parent;
    }
    const OperationResult recovered = recoverAtomicSdFile(localPath);
    if (!recovered.success) {
        return recovered;
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
    LIBSSH2_SFTP_ATTRIBUTES attributes = {};
    int statResult = LIBSSH2_ERROR_EAGAIN;
    const std::uint32_t statDeadline = millis() + timeoutMs;
    while (statResult == LIBSSH2_ERROR_EAGAIN &&
           static_cast<std::int32_t>(statDeadline - millis()) > 0) {
        statResult = libssh2_sftp_fstat(remote, &attributes);
        if (statResult == LIBSSH2_ERROR_EAGAIN) {
            delay(5);
        }
    }
    if (statResult != 0) {
        closeSftpHandle(remote);
        return {false, sessionError(implementation_->session, "SFTP remote file stat",
                                    statResult)};
    }
    const bool expectedSizeKnown =
        (attributes.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0;
    std::uint32_t expectedSize = 0;
    if (expectedSizeKnown) {
        if (attributes.filesize > kMaximumWorkspaceFileBytes) {
            closeSftpHandle(remote);
            return {false, "SFTP file exceeds the supported 32-bit file range"};
        }
        const OperationResult space = checkSdOperationSpace(
            attributes.filesize, kStorageOperationalFloorBytes);
        if (!space.success) {
            closeSftpHandle(remote);
            return space;
        }
        expectedSize = static_cast<std::uint32_t>(attributes.filesize);
    }
    const String temporaryName = workspaceName + ".tmp";
    const String temporaryPath = workspaceFilePath(temporaryName);
    SD.remove(temporaryPath);
    File local = SD.open(temporaryPath, FILE_WRITE);
    if (!local) {
        closeSftpHandle(remote);
        return {false, "Failed to create the SFTP download file on microSD"};
    }
    std::uint32_t total = 0;
    std::uint8_t buffer[1024] = {};
    std::uint32_t deadline = millis() + timeoutMs;
    bool completed = false;
    while (static_cast<std::int32_t>(deadline - millis()) > 0) {
        const ssize_t readBytes = libssh2_sftp_read(
            remote, reinterpret_cast<char*>(buffer), sizeof(buffer));
        if (readBytes > 0) {
            const std::size_t blockBytes = static_cast<std::size_t>(readBytes);
            if (blockBytes > kMaximumWorkspaceFileBytes - total) {
                local.close();
                closeSftpHandle(remote);
                SD.remove(temporaryPath);
                return {false, "SFTP file exceeds the supported 32-bit file range"};
            }
            if (!expectedSizeKnown) {
                const OperationResult space = checkSdOperationSpace(
                    blockBytes, kStorageOperationalFloorBytes);
                if (!space.success) {
                    local.close();
                    closeSftpHandle(remote);
                    SD.remove(temporaryPath);
                    return space;
                }
            }
            if (local.write(buffer, blockBytes) != blockBytes) {
                local.close();
                closeSftpHandle(remote);
                SD.remove(temporaryPath);
                return {false, "microSD rejected SFTP download data"};
            }
            total += static_cast<std::uint32_t>(blockBytes);
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
    if (expectedSizeKnown && total != expectedSize) {
        SD.remove(temporaryPath);
        return {false, "SFTP download size differs from the remote stat result"};
    }
    const OperationResult committed = commitWorkspaceBinaryTemporary(
        workspaceName, temporaryName);
    if (!committed.success) {
        SD.remove(temporaryPath);
        return committed;
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
    if (!isValidWorkspaceFilename(workspaceName.c_str())) {
        return {false, "SFTP upload source filename is invalid"};
    }
    const String localPath = workspaceFilePath(workspaceName);
    File local = SD.open(localPath, FILE_READ);
    if (!local || local.isDirectory()) {
        if (local) local.close();
        return {false, "SFTP upload source could not be opened from the workspace"};
    }
    const std::size_t localBytesValue = local.size();
    if (localBytesValue > kMaximumWorkspaceFileBytes) {
        local.close();
        return {false, "SFTP upload source exceeds the supported 32-bit file range"};
    }
    const std::uint32_t localBytes = static_cast<std::uint32_t>(localBytesValue);
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
    std::uint32_t transferredBytes = 0;
    while (transferredBytes < localBytes) {
        const std::size_t blockBytes = std::min<std::size_t>(
            sizeof(buffer), localBytes - transferredBytes);
        const std::size_t readBytes = local.read(buffer, blockBytes);
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
        transferredBytes += static_cast<std::uint32_t>(written);
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
