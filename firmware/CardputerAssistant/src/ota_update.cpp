#include "ota_update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace cardputer {
namespace {

constexpr const char* kLatestReleaseUrl =
    "https://api.github.com/repos/varlolwut/CardMind/releases/latest";
constexpr const char* kAssetName = "CardMind-cardputer-adv.bin";
constexpr const char* kAllowedAssetPrefix =
    "https://github.com/varlolwut/CardMind/releases/download/";
constexpr const char* kTemporaryFirmwarePath = "/assistant/update.bin.tmp";
constexpr const char* kFirmwarePath = "/assistant/update.bin";
constexpr std::uint32_t kMaximumFirmwareBytes = 0x330000U;
constexpr std::uint32_t kTransferTimeoutMs = 30000U;

const char kGithubRoots[] PROGMEM = R"CERT(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KKN
FtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5O
RAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7UrT
kXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdCj
NPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVco
yi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq4
RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPAm
RGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57de
myPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL
MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl
eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT
JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx
MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg
VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm
aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo
I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng
o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G
A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB
zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW
RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----)CERT";

bool parseVersion(const String& value, std::array<std::uint32_t, 3>& parts)
{
    String normalized = value;
    if (normalized.startsWith("v")) {
        normalized.remove(0, 1);
    }
    int start = 0;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const int separator = normalized.indexOf('.', start);
        const int end = separator >= 0 ? separator : normalized.length();
        if (end <= start || (index < parts.size() - 1 && separator < 0)) {
            return false;
        }
        const String token = normalized.substring(start, end);
        for (std::size_t character = 0; character < token.length(); ++character) {
            if (token[character] < '0' || token[character] > '9') {
                return false;
            }
        }
        parts[index] = static_cast<std::uint32_t>(token.toInt());
        start = end + 1;
    }
    return start > normalized.length();
}

bool validSha256(const String& value)
{
    if (value.length() != 64) {
        return false;
    }
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

String sha256Hex(const std::uint8_t* digest)
{
    char output[65] = {};
    for (std::size_t index = 0; index < 32; ++index) {
        std::snprintf(output + index * 2, 3, "%02x", digest[index]);
    }
    return String(output);
}

bool rollbackPartitionsPresent()
{
    const esp_partition_t* first = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    const esp_partition_t* second = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    return first != nullptr && second != nullptr;
}

OperationResult removePath(const char* path)
{
    return !SD.exists(path) || SD.remove(path)
        ? OperationResult{true, ""}
        : OperationResult{false, String("Failed to remove ") + path};
}

}  // namespace

bool isNewerFirmwareVersion(const String& candidate, const String& current)
{
    std::array<std::uint32_t, 3> candidateParts = {};
    std::array<std::uint32_t, 3> currentParts = {};
    if (!parseVersion(candidate, candidateParts) || !parseVersion(current, currentParts)) {
        return false;
    }
    return candidateParts > currentParts;
}

FirmwareUpdateInfo checkLatestFirmwareUpdate(const String& currentVersion)
{
    WiFiClientSecure client;
    client.setCACert(kGithubRoots);
    client.setHandshakeTimeout(20);
    HTTPClient http;
    http.setTimeout(30000);
    if (!http.begin(client, kLatestReleaseUrl)) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "Failed to initialize the GitHub release request"};
    }
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    http.addHeader("User-Agent", "CardMind-Firmware-Updater");
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        char tlsError[160] = {};
        client.lastError(tlsError, sizeof(tlsError));
        http.end();
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                status > 0 ? String("GitHub release request returned HTTP ") + status
                           : String("GitHub release transport failed: ") +
                                 HTTPClient::errorToString(status) +
                                 (tlsError[0] == '\0' ? "" : String("; TLS: ") + tlsError)};
    }
    const int declaredBytes = http.getSize();
    if (declaredBytes > 32768) {
        http.end();
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "GitHub latest release response exceeded 32768 bytes"};
    }
    const String responseBody = http.getString();
    http.end();
    if (responseBody.isEmpty() || responseBody.length() > 32768) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                responseBody.isEmpty()
                    ? "GitHub latest release response body was empty"
                    : "GitHub latest release response exceeded 32768 bytes"};
    }
    JsonDocument release;
    const DeserializationError error = deserializeJson(release, responseBody);
    if (error) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                String("GitHub latest release JSON parsing failed: ") + error.c_str()};
    }
    if (!release["tag_name"].is<const char*>()) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "GitHub latest release response is missing tag_name"};
    }
    if (!release["assets"].is<JsonArrayConst>()) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "GitHub latest release response is missing the assets array"};
    }
    String assetUrl;
    String digest;
    std::uint32_t assetBytes = 0;
    for (const JsonObjectConst asset : release["assets"].as<JsonArrayConst>()) {
        if (asset["name"].is<const char*>() &&
            String(asset["name"].as<const char*>()) == kAssetName &&
            asset["browser_download_url"].is<const char*>() &&
            asset["size"].is<std::uint32_t>() && asset["digest"].is<const char*>()) {
            assetUrl = asset["browser_download_url"].as<const char*>();
            assetBytes = asset["size"].as<std::uint32_t>();
            digest = asset["digest"].as<const char*>();
            break;
        }
    }
    if (!assetUrl.startsWith(kAllowedAssetPrefix) || assetBytes == 0 ||
        assetBytes > kMaximumFirmwareBytes || !digest.startsWith("sha256:")) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "Latest release is missing a valid signed Cardputer ADV firmware asset"};
    }
    digest.remove(0, 7);
    digest.toLowerCase();
    if (!validSha256(digest)) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "Latest firmware asset has an invalid SHA-256 digest"};
    }
    const String version = release["tag_name"].as<const char*>();
    std::array<std::uint32_t, 3> parsed = {};
    if (!parseVersion(version, parsed)) {
        return {false, false, "", "", "", 0, rollbackPartitionsPresent(),
                "Latest release tag is not a stable semantic version"};
    }
    return {true, isNewerFirmwareVersion(version, currentVersion), version, assetUrl,
            digest, assetBytes, rollbackPartitionsPresent(), ""};
}

OperationResult downloadFirmwareUpdate(const FirmwareUpdateInfo& info,
                                       FirmwareProgressCallback onProgress,
                                       FirmwareCancelCallback isCancelled)
{
    if (!info.success || !info.newerAvailable || !validSha256(info.sha256) ||
        !info.assetUrl.startsWith(kAllowedAssetPrefix) || info.assetBytes == 0 ||
        info.assetBytes > kMaximumFirmwareBytes) {
        return {false, "Firmware update metadata is invalid or not newer"};
    }
    OperationResult result = removePath(kTemporaryFirmwarePath);
    if (result.success) {
        result = removePath(kFirmwarePath);
    }
    if (!result.success) {
        return result;
    }
    WiFiClientSecure client;
    client.setCACert(kGithubRoots);
    client.setHandshakeTimeout(20);
    HTTPClient http;
    http.setTimeout(30000);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, info.assetUrl)) {
        return {false, "Failed to initialize the firmware download"};
    }
    http.addHeader("Accept", "application/octet-stream");
    http.addHeader("User-Agent", "CardMind-Firmware-Updater");
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        char tlsError[160] = {};
        client.lastError(tlsError, sizeof(tlsError));
        http.end();
        return {false, status > 0 ? String("Firmware download returned HTTP ") + status
                                  : String("Firmware download transport failed: ") +
                                        HTTPClient::errorToString(status) +
                                        (tlsError[0] == '\0'
                                            ? ""
                                            : String("; TLS: ") + tlsError)};
    }
    const int declaredBytes = http.getSize();
    if (declaredBytes >= 0 && static_cast<std::uint32_t>(declaredBytes) != info.assetBytes) {
        http.end();
        return {false, "Firmware download size does not match GitHub release metadata"};
    }
    File output = SD.open(kTemporaryFirmwarePath, FILE_WRITE);
    if (!output) {
        http.end();
        return {false, "Failed to create the temporary firmware file on microSD"};
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts(&sha, 0) != 0) {
        output.close();
        http.end();
        mbedtls_sha256_free(&sha);
        removePath(kTemporaryFirmwarePath);
        return {false, "Failed to initialize firmware SHA-256 verification"};
    }
    NetworkClient* stream = http.getStreamPtr();
    std::array<std::uint8_t, 4096> buffer = {};
    std::uint32_t total = 0;
    std::uint32_t lastDataAt = millis();
    while (total < info.assetBytes) {
        if (isCancelled()) {
            result = {false, "Firmware download canceled by user"};
            break;
        }
        const int available = stream->available();
        if (available <= 0) {
            if (!http.connected() || millis() - lastDataAt >= kTransferTimeoutMs) {
                result = {false, "Firmware download ended before all bytes arrived"};
                break;
            }
            delay(2);
            continue;
        }
        const std::size_t requested = std::min<std::size_t>(
            buffer.size(), std::min<std::uint32_t>(available, info.assetBytes - total));
        const std::size_t received = stream->readBytes(buffer.data(), requested);
        if (received == 0 || output.write(buffer.data(), received) != received ||
            mbedtls_sha256_update(&sha, buffer.data(), received) != 0) {
            result = {false, "Failed while streaming firmware to microSD"};
            break;
        }
        total += static_cast<std::uint32_t>(received);
        lastDataAt = millis();
        onProgress(total, info.assetBytes);
    }
    std::uint8_t digest[32] = {};
    if (result.success && mbedtls_sha256_finish(&sha, digest) != 0) {
        result = {false, "Failed to finalize firmware SHA-256 verification"};
    }
    mbedtls_sha256_free(&sha);
    output.flush();
    output.close();
    http.end();
    if (result.success && sha256Hex(digest) != info.sha256) {
        result = {false, "Downloaded firmware SHA-256 does not match GitHub metadata"};
    }
    if (!result.success) {
        removePath(kTemporaryFirmwarePath);
        return result;
    }
    if (!SD.rename(kTemporaryFirmwarePath, kFirmwarePath)) {
        removePath(kTemporaryFirmwarePath);
        return {false, "Failed to commit the verified firmware file on microSD"};
    }
    return {true, ""};
}

OperationResult installDownloadedFirmware(const FirmwareUpdateInfo& info,
                                          FirmwareProgressCallback onProgress,
                                          FirmwareCancelCallback isCancelled)
{
    File input = SD.open(kFirmwarePath, FILE_READ);
    if (!input || input.size() != info.assetBytes) {
        if (input) {
            input.close();
        }
        return {false, "Verified firmware file is missing or has the wrong size"};
    }
    if (!Update.begin(info.assetBytes, U_FLASH)) {
        input.close();
        return {false, String("OTA partition rejected the firmware: ") + Update.errorString()};
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts(&sha, 0) != 0) {
        Update.abort();
        input.close();
        mbedtls_sha256_free(&sha);
        return {false, "Failed to initialize installation SHA-256 verification"};
    }
    std::array<std::uint8_t, 4096> buffer = {};
    std::uint32_t total = 0;
    OperationResult result = {true, ""};
    while (input.available() && result.success) {
        if (isCancelled()) {
            result = {false, "Firmware installation canceled by user"};
            break;
        }
        const std::size_t received = input.read(buffer.data(), buffer.size());
        if (received == 0 || Update.write(buffer.data(), received) != received ||
            mbedtls_sha256_update(&sha, buffer.data(), received) != 0) {
            result = {false, String("Failed while writing OTA partition: ") +
                             Update.errorString()};
            break;
        }
        total += static_cast<std::uint32_t>(received);
        onProgress(total, info.assetBytes);
    }
    std::uint8_t digest[32] = {};
    if (result.success && (total != info.assetBytes ||
        mbedtls_sha256_finish(&sha, digest) != 0 || sha256Hex(digest) != info.sha256)) {
        result = {false, "Installed firmware failed final size or SHA-256 verification"};
    }
    mbedtls_sha256_free(&sha);
    input.close();
    if (!result.success) {
        Update.abort();
        return result;
    }
    if (!Update.end(false) || !Update.isFinished()) {
        return {false, String("Failed to activate the OTA partition: ") + Update.errorString()};
    }
    return {true, ""};
}

OperationResult removeDownloadedFirmware()
{
    OperationResult result = removePath(kTemporaryFirmwarePath);
    return result.success ? removePath(kFirmwarePath) : result;
}

}  // namespace cardputer
