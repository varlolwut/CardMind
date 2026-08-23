#include "ssh_client.h"

#include <libssh2.h>
#define public public_key
#include <libssh2_priv.h>
#undef public

#include <NetworkClient.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <sys/socket.h>

namespace cardputer {
namespace {

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

}  // namespace

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

    size_t hostKeyLength = 0;
    int hostKeyType = 0;
    const char* hostKey = libssh2_session_hostkey(session, &hostKeyLength, &hostKeyType);
    const unsigned char* hash = reinterpret_cast<const unsigned char*>(
        libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256));
    if (hostKey == nullptr || hostKeyLength == 0 || hash == nullptr) {
        libssh2_session_disconnect(session, "CardMind host-key probe complete");
        libssh2_session_free(session);
        client.stop();
        libssh2_exit();
        return {false, "", "", "SSH server did not provide a usable SHA-256 host key"};
    }

    char fingerprint[96] = {};
    std::size_t position = 0;
    for (std::size_t index = 0; index < 32; ++index) {
        const int written = std::snprintf(fingerprint + position,
                                          sizeof(fingerprint) - position,
                                          index == 0 ? "%02X" : ":%02X", hash[index]);
        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(fingerprint) - position) {
            libssh2_session_disconnect(session, "CardMind host-key probe complete");
            libssh2_session_free(session);
            client.stop();
            libssh2_exit();
            return {false, "", "", "SSH SHA-256 fingerprint formatting failed"};
        }
        position += static_cast<std::size_t>(written);
    }
    const String type = hostKeyType == LIBSSH2_HOSTKEY_TYPE_RSA
        ? String("RSA")
        : (hostKeyType == LIBSSH2_HOSTKEY_TYPE_DSS
            ? String("DSA")
            : (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_256
                ? String("ECDSA-256")
                : (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_384
                    ? String("ECDSA-384")
                    : (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ECDSA_521
                        ? String("ECDSA-521")
                        : (hostKeyType == LIBSSH2_HOSTKEY_TYPE_ED25519
                            ? String("ED25519") : String("unknown"))))));

    libssh2_session_disconnect(session, "CardMind host-key probe complete");
    libssh2_session_free(session);
    client.stop();
    libssh2_exit();
    return {true, String(fingerprint), type, ""};
}

}  // namespace cardputer
