#pragma once

#include "app_types.h"

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cardputer {

constexpr std::size_t kMaximumStorageIndexLineBytes = 1024;
constexpr std::uint64_t kStorageOperationalFloorBytes = 1048576;

struct StorageLinesPageResult {
    bool success;
    std::vector<String> lines;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct StorageIndexLookupResult {
    bool success;
    bool found;
    String line;
    String error;
};

OperationResult ensureSdDirectory(const String& path);
OperationResult recoverAtomicSdFile(const String& target);
OperationResult writeAtomicJsonSdFile(const String& target, JsonDocument& document);
OperationResult writeEmptyAtomicSdFile(const String& target);
OperationResult commitStagedSdFile(const String& target, const String& staged);
OperationResult copySdFileAtomically(const String& source,
                                     const String& target,
                                     std::uint64_t operationalFloorBytes);
OperationResult checkSdOperationSpace(std::uint64_t requiredBytes,
                                      std::uint64_t operationalFloorBytes);
OperationResult mutateJsonlSdIndex(const String& path,
                                   const String& keyField,
                                   const String& keyValue,
                                   const String& replacementLine,
                                   bool removeEntry,
                                   std::uint64_t operationalFloorBytes);
StorageLinesPageResult readJsonlSdIndexPage(const String& path,
                                            std::uint32_t offset,
                                            std::size_t maximumEntries);
StorageIndexLookupResult findJsonlSdIndexEntry(const String& path,
                                               const String& keyField,
                                               const String& keyValue);
OperationResult removeSdDirectoryTree(const String& path);

}  // namespace cardputer
