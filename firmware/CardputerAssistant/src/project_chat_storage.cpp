#include "project_chat_storage.h"

#include "chat_storage.h"
#include "file_workspace.h"
#include "json_string_reader.h"
#include "project_storage.h"
#include "sd_storage.h"
#include "storage.h"
#include "text_utils.h"
#include "tool_policy_codec.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cardputer {

ProjectChatAppendPlanResult planProjectChatAppend(
    std::uint64_t currentHistoryBytes,
    std::uint64_t appendedHistoryBytes,
    std::uint64_t historyQuotaBytes,
    std::uint64_t markerBytes,
    std::uint64_t stagedTailBytes,
    std::uint64_t stagedMetadataBytes,
    std::uint64_t freeBytes,
    std::uint64_t operationalFloorBytes)
{
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (historyQuotaBytes != 0 &&
        historyQuotaBytes < kMinimumProjectChatHistoryQuotaBytes) {
        return {false, 0, 0,
                "Project chat history quota must be 0 or at least 2 MiB"};
    }
    if (currentHistoryBytes > maximum - appendedHistoryBytes) {
        return {false, 0, 0, "Project chat raw history byte count overflows"};
    }
    const std::uint64_t newHistoryBytes =
        currentHistoryBytes + appendedHistoryBytes;
    if (newHistoryBytes > std::numeric_limits<std::uint32_t>::max()) {
        return {false, newHistoryBytes, 0,
                "Project chat raw history exceeds the 32-bit filesystem cursor range"};
    }
    if (historyQuotaBytes != 0 && newHistoryBytes > historyQuotaBytes) {
        return {false, newHistoryBytes, 0,
                "Project chat raw history quota would be exceeded"};
    }
    std::uint64_t requiredFreeBytes = markerBytes;
    if (requiredFreeBytes > maximum - appendedHistoryBytes) {
        return {false, newHistoryBytes, 0,
                "Project chat append space calculation overflows at raw history"};
    }
    requiredFreeBytes += appendedHistoryBytes;
    if (requiredFreeBytes > maximum - stagedTailBytes) {
        return {false, newHistoryBytes, 0,
                "Project chat append space calculation overflows at staged tail"};
    }
    requiredFreeBytes += stagedTailBytes;
    if (requiredFreeBytes > maximum - stagedMetadataBytes) {
        return {false, newHistoryBytes, 0,
                "Project chat append space calculation overflows at staged metadata"};
    }
    requiredFreeBytes += stagedMetadataBytes;
    if (requiredFreeBytes > maximum - operationalFloorBytes) {
        return {false, newHistoryBytes, 0,
                "Project chat append space calculation overflows at operational floor"};
    }
    requiredFreeBytes += operationalFloorBytes;
    if (requiredFreeBytes > freeBytes) {
        return {false, newHistoryBytes, requiredFreeBytes,
                "microSD does not have enough free space for the project chat append transaction"};
    }
    return {true, newHistoryBytes, requiredFreeBytes, ""};
}

namespace {

constexpr std::size_t kMaximumProjectChatModelBytes = 240;

String projectChatMetadataPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/chat.json";
}

String projectChatHistoryPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/history.jsonl";
}

String projectChatTailPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/tail.jsonl";
}

String projectChatAppendMarkerPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/append.pending.json";
}

std::size_t unsignedDecimalBytes(std::uint64_t value)
{
    std::size_t bytes = 1;
    while (value >= 10) {
        value /= 10;
        ++bytes;
    }
    return bytes;
}

StorageSizeResult measureUpdatedProjectChatMetadata(
    const ChatDocument& chat,
    std::uint32_t messageCount,
    std::uint64_t updatedAt,
    std::uint32_t revision)
{
    const ToolPolicyEncodeResult policy =
        encodeScopedToolPermissionPolicy(chat.toolPolicy);
    if (policy.error != ToolPolicyCodecError::None) {
        return {
            false,
            0,
            String("Project chat tool policy cannot be measured: ") +
                toolPolicyCodecErrorText(policy.error),
        };
    }
    JsonDocument document;
    document["version"] = kProjectChatFormatVersion;
    document["project_id"] = chat.projectId;
    document["id"] = chat.summary.id;
    document["title"] = chat.summary.title;
    document["updated_at"] = updatedAt;
    document["message_count"] = messageCount;
    document["pinned"] = chat.summary.pinned;
    document["archived"] = chat.summary.archived;
    document["revision"] = revision;
    document["instructions"] = chat.instructions.c_str();
    document["draft"] = chat.draft.c_str();
    document["tool_policy"] = policy.encoded.value.data();
    document["ssh_tools_enabled"] = legacySshToolsEnabled(chat.toolPolicy);
    document["ssh_profile"] = chat.sshProfile;
    document["context_summary"] = chat.contextSummary.c_str();
    document["summarized_message_count"] = chat.summarizedMessageCount;
    document["model"] = chat.model;
    return {true, measureJson(document), ""};
}

String serializeUpdatedChatSummary(const ChatSummary& source,
                                   std::uint32_t messageCount,
                                   std::uint64_t updatedAt,
                                   std::uint32_t revision)
{
    JsonDocument document;
    document["id"] = source.id;
    document["title"] = source.title;
    document["updated_at"] = updatedAt;
    document["message_count"] = messageCount;
    document["archived_message_count"] = source.archivedMessageCount;
    document["pinned"] = source.pinned;
    document["archived"] = source.archived;
    document["revision"] = revision;
    String line;
    serializeJson(document, line);
    return line;
}

String serializeWorstCaseProjectSummary(const ProjectSummary& source,
                                        std::uint32_t revision)
{
    JsonDocument document;
    document["id"] = source.id;
    document["title"] = source.title;
    document["updated_at"] = std::numeric_limits<std::uint64_t>::max();
    document["chat_count"] = source.chatCount;
    document["pinned"] = source.pinned;
    document["archived"] = source.archived;
    document["revision"] = revision;
    String line;
    serializeJson(document, line);
    return line;
}

constexpr std::size_t kStoredTailMessages = 96;
constexpr std::size_t kStoredTailBytes = 131072;
constexpr std::size_t kMaximumSerializedTailRecordBytes = 6 * kStoredTailBytes + 128;

class BufferedFileReader {
public:
    explicit BufferedFileReader(File& file)
        : file_(file), bufferOffset_(0), bufferSize_(0), recordBytes_(0),
          failed_(false), recordLimitExceeded_(false)
    {
    }

    BufferedFileReader(const BufferedFileReader&) = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    bool prepareRecord()
    {
        while (peekBuffered() == '\n') {
            readBuffered();
        }
        recordBytes_ = 0;
        recordLimitExceeded_ = false;
        return peekBuffered() >= 0;
    }

    int read()
    {
        if (recordBytes_ >= kMaximumSerializedTailRecordBytes) {
            recordLimitExceeded_ = true;
            return -1;
        }
        if (peekBuffered() == '\n') {
            return -1;
        }
        const int value = readBuffered();
        if (value >= 0) {
            ++recordBytes_;
        }
        return value;
    }

    int available()
    {
        return peek() >= 0 ? 1 : 0;
    }

    int peek()
    {
        if (recordBytes_ >= kMaximumSerializedTailRecordBytes) {
            recordLimitExceeded_ = true;
            return -1;
        }
        const int value = peekBuffered();
        return value == '\n' ? -1 : value;
    }

    std::size_t position() const
    {
        const std::size_t physicalPosition = file_.position();
        const std::size_t unreadBytes = bufferSize_ - bufferOffset_;
        return physicalPosition >= unreadBytes ? physicalPosition - unreadBytes : 0;
    }

    bool seek(std::size_t position)
    {
        if (failed_ || !file_.seek(position)) {
            return false;
        }
        bufferOffset_ = 0;
        bufferSize_ = 0;
        recordBytes_ = 0;
        recordLimitExceeded_ = false;
        return true;
    }

    std::size_t readBytes(char* output, std::size_t maximumBytes)
    {
        std::size_t copied = 0;
        while (copied < maximumBytes) {
            const int value = read();
            if (value < 0) {
                break;
            }
            output[copied++] = static_cast<char>(value);
        }
        return copied;
    }

    bool consumeRecordDelimiter()
    {
        for (;;) {
            const int value = peekBuffered();
            if (value < 0) {
                return !failed_;
            }
            if (value == '\n') {
                return readBuffered() >= 0;
            }
            if (value == ' ' || value == '\t' || value == '\r') {
                if (readRecordByte() < 0) {
                    return false;
                }
                continue;
            }
            return false;
        }
    }

    bool failed() const
    {
        return failed_;
    }

    bool recordLimitExceeded() const
    {
        return recordLimitExceeded_;
    }

private:
    bool refill()
    {
        if (bufferOffset_ < bufferSize_) {
            return true;
        }
        bufferOffset_ = 0;
        bufferSize_ = 0;
        const std::size_t totalBytes = file_.size();
        const std::size_t position = file_.position();
        if (position >= totalBytes) {
            return false;
        }
        const std::size_t requested = std::min<std::size_t>(
            totalBytes - position, sizeof(buffer_));
        const std::size_t received = file_.read(buffer_, requested);
        if (received != requested) {
            failed_ = true;
            return false;
        }
        bufferSize_ = received;
        return true;
    }

    int peekBuffered()
    {
        return refill() ? static_cast<int>(buffer_[bufferOffset_]) : -1;
    }

    int readBuffered()
    {
        if (!refill()) {
            return -1;
        }
        return static_cast<int>(buffer_[bufferOffset_++]);
    }

    int readRecordByte()
    {
        if (recordBytes_ >= kMaximumSerializedTailRecordBytes) {
            recordLimitExceeded_ = true;
            return -1;
        }
        const int value = readBuffered();
        if (value >= 0) {
            ++recordBytes_;
        }
        return value;
    }

    File& file_;
    std::uint8_t buffer_[512];
    std::size_t bufferOffset_;
    std::size_t bufferSize_;
    std::size_t recordBytes_;
    bool failed_;
    bool recordLimitExceeded_;
};

enum class TailRecordSource : std::uint8_t {
    ExistingTail,
    RawHistory,
};

struct TailRecordSpan {
    TailRecordSource source;
    std::size_t offset;
    std::size_t contentBytes;
};

struct TailRetentionWindow {
    TailRecordSpan records[kStoredTailMessages];
    std::size_t first;
    std::size_t count;
    std::size_t contentBytes;
};

struct TailSourceScanResult {
    bool success;
    TailRetentionWindow retained;
    std::uint32_t firstSequence;
    std::uint32_t lastSequence;
    std::uint32_t recordCount;
    std::size_t sourceBytes;
    String error;
};

struct ProjectChatMetadataStateResult {
    bool success;
    std::uint32_t messageCount;
    std::uint32_t revision;
    String error;
};

OperationResult validateHistoryFile(const String& path, std::uint32_t expectedMessages);
OperationResult recoverPendingAppend(const String& projectId, const String& chatId);
ChatDocumentResult loadProjectChatMetadataDocument(
    const String& projectId,
    const String& chatId);

String generateProjectChatId()
{
    char buffer[25];
    std::snprintf(buffer, sizeof(buffer), "%08lx%08lx",
                  static_cast<unsigned long>(esp_random()),
                  static_cast<unsigned long>(esp_random()));
    return String(buffer);
}

OperationResult validateProjectChatMetadataValues(const ChatDocument& chat)
{
    if (!isValidChatId(chat.projectId.c_str()) ||
        !isValidChatId(chat.summary.id.c_str())) {
        return {false, "Project chat metadata contains an invalid id"};
    }
    if (chat.summary.title.isEmpty() || !isValidUtf8(chat.summary.title.c_str())) {
        return {false, "Project chat title must be non-empty valid UTF-8"};
    }
    if (chat.instructions.size() > kMaximumProjectChatInstructionsBytes ||
        chat.draft.size() > kMaximumProjectChatDraftBytes ||
        chat.contextSummary.size() > kMaximumProjectChatSummaryBytes ||
        !isValidUtf8(chat.instructions) || !isValidUtf8(chat.draft) ||
        !isValidUtf8(chat.contextSummary)) {
        return {false, "Project chat text metadata exceeds its limit or is invalid UTF-8"};
    }
    if (chat.model.length() > kMaximumProjectChatModelBytes ||
        !isValidUtf8(chat.model.c_str())) {
        return {false, "Project chat model must be valid UTF-8 up to 240 bytes"};
    }
    if (chat.sshProfile.length() > 120 ||
        !isValidUtf8(chat.sshProfile.c_str())) {
        return {false, "Project chat SSH profile must be valid UTF-8 up to 120 bytes"};
    }
    if (chat.summarizedMessageCount > chat.summary.messageCount) {
        return {false, "Project chat summary covers more messages than the chat contains"};
    }
    const ToolPolicyEncodeResult policy =
        encodeScopedToolPermissionPolicy(chat.toolPolicy);
    if (policy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project chat tool policy is invalid: ") +
                toolPolicyCodecErrorText(policy.error),
        };
    }
    return {true, ""};
}

OperationResult writeHistoryMessage(File& file,
                                    std::uint32_t sequence,
                                    const Message& message)
{
    if ((message.role != "user" && message.role != "assistant") ||
        message.content.empty() || !isValidUtf8(message.content)) {
        return {false, "Cannot write invalid raw chat history message"};
    }
    JsonDocument item;
    item["sequence"] = sequence;
    item["role"] = message.role;
    item["content"] = message.content;
    const std::size_t expectedBytes = measureJson(item);
    if (serializeJson(item, file) != expectedBytes || file.write('\n') != 1) {
        return {false, "Failed to write complete raw chat history message"};
    }
    return {true, ""};
}

void retainTailRecord(TailRetentionWindow& retained,
                      const TailRecordSpan& record)
{
    if (retained.count == kStoredTailMessages) {
        retained.contentBytes -= retained.records[retained.first].contentBytes;
        retained.first = (retained.first + 1) % kStoredTailMessages;
        --retained.count;
    }
    const std::size_t insertion =
        (retained.first + retained.count) % kStoredTailMessages;
    retained.records[insertion] = record;
    retained.contentBytes += record.contentBytes;
    ++retained.count;
    while (retained.count > 0 && retained.contentBytes > kStoredTailBytes) {
        retained.contentBytes -= retained.records[retained.first].contentBytes;
        retained.first = (retained.first + 1) % kStoredTailMessages;
        --retained.count;
    }
}

TailSourceScanResult scanTailSource(const String& path,
                                    std::size_t firstByte,
                                    TailRecordSource source)
{
    TailRetentionWindow retained = {};
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, {}, 0, 0, 0, 0, "Project chat tail source does not exist"};
    }
    const std::size_t sourceBytes = file.size();
    if (firstByte > sourceBytes || !file.seek(firstByte)) {
        file.close();
        return {false, {}, 0, 0, 0, sourceBytes,
                "Project chat tail source offset is outside the file"};
    }
    BufferedFileReader reader(file);
    std::uint32_t firstSequence = 0;
    std::uint32_t lastSequence = 0;
    std::uint32_t recordCount = 0;
    JsonDocument headerFilter;
    headerFilter["sequence"] = true;
    headerFilter["role"] = true;
    while (reader.prepareRecord()) {
        const std::size_t recordOffset = reader.position();
        JsonDocument header;
        const DeserializationError headerError = deserializeJson(
            header, reader, DeserializationOption::Filter(headerFilter));
        const std::uint32_t sequence = headerError
            ? 0 : header["sequence"].as<std::uint32_t>();
        const char* role = headerError ? nullptr : header["role"].as<const char*>();
        if (reader.failed() || reader.recordLimitExceeded() || headerError ||
            !header["sequence"].is<std::uint32_t>() || sequence == 0 ||
            (lastSequence != 0 && sequence != lastSequence + 1) ||
            role == nullptr ||
            (std::strcmp(role, "user") != 0 && std::strcmp(role, "assistant") != 0) ||
            !reader.seek(recordOffset)) {
            file.close();
            return {false, {}, 0, 0, 0, sourceBytes,
                    "Project chat tail source contains an invalid record"};
        }
        const json_reader::JsonStringLengthResult content =
            json_reader::measureObjectStringField(
                reader, "content", 64, kStoredTailBytes);
        if (!content.success) {
            file.close();
            return {false, {}, 0, 0, 0, sourceBytes,
                    "Project chat tail source content is invalid: " + content.error};
        }
        if (content.bytes == 0 || !reader.consumeRecordDelimiter()) {
            file.close();
            return {false, {}, 0, 0, 0, sourceBytes,
                    "Project chat tail source content or delimiter is invalid"};
        }
        if (recordCount == 0) {
            firstSequence = sequence;
        }
        lastSequence = sequence;
        ++recordCount;
        retainTailRecord(retained, {source, recordOffset, content.bytes});
    }
    const bool validEnd = !reader.failed() && !reader.recordLimitExceeded();
    file.close();
    return validEnd
        ? TailSourceScanResult{true, retained, firstSequence, lastSequence,
                               recordCount, sourceBytes, ""}
        : TailSourceScanResult{false, {}, 0, 0, 0, sourceBytes,
                               "Failed to finish scanning the project chat tail source"};
}

OperationResult copyFileSuffix(File& output,
                               const String& sourcePath,
                               std::size_t firstByte)
{
    File source = SD.open(sourcePath, FILE_READ);
    if (!source) {
        return {false, "Project chat tail copy source does not exist"};
    }
    if (firstByte > source.size() || !source.seek(firstByte)) {
        source.close();
        return {false, "Project chat tail copy offset is outside the source"};
    }
    OperationResult result = {true, ""};
    std::uint8_t buffer[512];
    while (source.available()) {
        const std::size_t received = source.read(buffer, sizeof(buffer));
        if (received == 0 || output.write(buffer, received) != received) {
            result = {false, "Failed to copy the complete project chat tail"};
            break;
        }
    }
    source.close();
    return result;
}

OperationResult writeTailSuffix(const String& historyPath,
                                const String& tailPath,
                                std::size_t firstRetainedOffset)
{
    OperationResult result = recoverAtomicSdFile(tailPath);
    if (!result.success) {
        return result;
    }
    const String stagedPath = tailPath + ".tmp";
    File staged = SD.open(stagedPath, FILE_WRITE);
    if (!staged) {
        return {false, "Failed to create staged project chat tail"};
    }
    result = copyFileSuffix(staged, historyPath, firstRetainedOffset);
    staged.flush();
    staged.close();
    if (!result.success) {
        SD.remove(stagedPath);
        return result;
    }
    return commitStagedSdFile(tailPath, stagedPath);
}

OperationResult updateTailAfterAppend(const String& projectId,
                                      const String& chatId,
                                      std::uint32_t oldMessageCount,
                                      std::uint32_t newMessageCount,
                                      std::size_t oldHistoryBytes)
{
    const String tailPath = projectChatTailPath(projectId, chatId);
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    OperationResult result = recoverAtomicSdFile(tailPath);
    if (!result.success) {
        return result;
    }
    TailSourceScanResult existing = scanTailSource(
        tailPath, 0, TailRecordSource::ExistingTail);
    if (!existing.success ||
        (oldMessageCount == 0 && existing.recordCount != 0) ||
        (oldMessageCount > 0 &&
         (existing.recordCount == 0 || existing.lastSequence != oldMessageCount))) {
        return {false, existing.success
            ? String("Existing project chat tail does not match metadata")
            : existing.error};
    }
    const TailSourceScanResult appended = scanTailSource(
        historyPath, oldHistoryBytes, TailRecordSource::RawHistory);
    if (!appended.success || appended.recordCount == 0 ||
        appended.firstSequence != oldMessageCount + 1 ||
        appended.lastSequence != newMessageCount ||
        appended.recordCount != newMessageCount - oldMessageCount) {
        return {false, appended.success
            ? String("Appended project chat history does not match its marker")
            : appended.error};
    }
    if (appended.retained.count < appended.recordCount) {
        existing.retained = appended.retained;
    } else {
        for (std::size_t index = 0; index < appended.retained.count; ++index) {
            const std::size_t slot =
                (appended.retained.first + index) % kStoredTailMessages;
            retainTailRecord(existing.retained, appended.retained.records[slot]);
        }
    }
    if (existing.retained.count == 0) {
        return {false, "Project chat append produced an empty context tail"};
    }
    const TailRecordSpan& first =
        existing.retained.records[existing.retained.first];
    std::uint64_t stagedTailBytes = 0;
    if (first.source == TailRecordSource::ExistingTail) {
        const std::uint64_t retainedExistingBytes =
            existing.sourceBytes - first.offset;
        const std::uint64_t appendedBytes =
            appended.sourceBytes - oldHistoryBytes;
        if (retainedExistingBytes >
            std::numeric_limits<std::uint64_t>::max() - appendedBytes) {
            return {false, "Project chat staged tail size overflows"};
        }
        stagedTailBytes = retainedExistingBytes + appendedBytes;
    } else {
        stagedTailBytes = appended.sourceBytes - first.offset;
    }
    result = checkSdOperationSpace(
        stagedTailBytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = recoverAtomicSdFile(tailPath);
    if (!result.success) {
        return result;
    }
    const String stagedPath = tailPath + ".tmp";
    File staged = SD.open(stagedPath, FILE_WRITE);
    if (!staged) {
        return {false, "Failed to create staged project chat tail"};
    }
    if (first.source == TailRecordSource::ExistingTail) {
        result = copyFileSuffix(staged, tailPath, first.offset);
        if (result.success) {
            result = copyFileSuffix(staged, historyPath, oldHistoryBytes);
        }
    } else {
        result = copyFileSuffix(staged, historyPath, first.offset);
    }
    staged.flush();
    staged.close();
    if (!result.success) {
        SD.remove(stagedPath);
        return result;
    }
    return commitStagedSdFile(tailPath, stagedPath);
}

OperationResult rebuildTailFile(const String& projectId,
                                const String& chatId,
                                std::uint32_t expectedMessages)
{
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    const TailSourceScanResult scanned = scanTailSource(
        historyPath, 0, TailRecordSource::RawHistory);
    if (!scanned.success) {
        return {false, scanned.error};
    }
    if (scanned.recordCount != expectedMessages ||
        (expectedMessages > 0 &&
         (scanned.firstSequence != 1 || scanned.lastSequence != expectedMessages))) {
        return {false, "Project chat raw history count changed while rebuilding its tail"};
    }
    const std::size_t firstRetainedOffset = scanned.retained.count == 0
        ? scanned.sourceBytes
        : scanned.retained.records[scanned.retained.first].offset;
    return writeTailSuffix(
        historyPath, projectChatTailPath(projectId, chatId), firstRetainedOffset);
}

struct LegacyHistoryMeasurement {
    bool success;
    std::uint64_t bytes;
    std::uint32_t messages;
    String error;
};

LegacyHistoryMeasurement measureLegacyHistory(const ChatDocument& legacyChat)
{
    std::uint64_t bytes = 0;
    std::uint32_t sequence = 1;
    std::uint32_t archiveOffset = 0;
    bool archiveEof = false;
    while (!archiveEof) {
        const ArchivedMessagesPageResult page = readArchivedChatMessages(
            legacyChat.summary.id, archiveOffset, 16, 32768);
        if (!page.success) {
            return {false, 0, 0, page.error};
        }
        for (const Message& message : page.messages) {
            JsonDocument item;
            item["sequence"] = sequence;
            item["role"] = message.role;
            item["content"] = message.content;
            bytes += measureJson(item) + 1;
            ++sequence;
        }
        archiveOffset = page.nextOffset;
        archiveEof = page.eof;
    }
    for (const Message& message : legacyChat.messages) {
        JsonDocument item;
        item["sequence"] = sequence;
        item["role"] = message.role;
        item["content"] = message.content;
        bytes += measureJson(item) + 1;
        ++sequence;
    }
    return {true, bytes, sequence - 1, ""};
}

ChatDocumentResult parseProjectChatMetadata(File& file,
                                            const String& projectId,
                                            const String& chatId,
                                            const String& path)
{
    JsonDocument filter;
    filter["version"] = true;
    filter["project_id"] = true;
    filter["id"] = true;
    filter["title"] = true;
    filter["updated_at"] = true;
    filter["message_count"] = true;
    filter["pinned"] = true;
    filter["archived"] = true;
    filter["revision"] = true;
    filter["tool_policy"] = true;
    filter["ssh_tools_enabled"] = true;
    filter["ssh_profile"] = true;
    filter["summarized_message_count"] = true;
    filter["model"] = true;
    JsonDocument document;
    const DeserializationError error = deserializeJson(
        document, file, DeserializationOption::Filter(filter));
    const JsonVariantConst model = document.as<JsonObjectConst>()["model"];
    const JsonVariantConst sshProfile =
        document.as<JsonObjectConst>()["ssh_profile"];
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != kProjectChatFormatVersion ||
        !document["project_id"].is<const char*>() || !document["id"].is<const char*>() ||
        !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() ||
        !document["message_count"].is<std::uint32_t>() ||
        !document["pinned"].is<bool>() || !document["archived"].is<bool>() ||
        !document["revision"].is<std::uint32_t>() ||
        !document["ssh_tools_enabled"].is<bool>() ||
        !document["summarized_message_count"].is<std::uint32_t>()) {
        return {false, {}, "Project chat metadata is missing required typed fields"};
    }
    if (!model.isUnbound() && !model.is<const char*>()) {
        return {false, {}, "Project chat model must be a string when present"};
    }
    if (!sshProfile.isUnbound() && !sshProfile.is<const char*>()) {
        return {false, {}, "Project chat ssh_profile must be a string when present"};
    }
    ChatDocument chat;
    chat.projectId = document["project_id"].as<const char*>();
    chat.summary.id = document["id"].as<const char*>();
    chat.summary.title = document["title"].as<const char*>();
    chat.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    chat.summary.messageCount = document["message_count"].as<std::uint32_t>();
    chat.summary.pinned = document["pinned"].as<bool>();
    chat.summary.archived = document["archived"].as<bool>();
    chat.summary.revision = document["revision"].as<std::uint32_t>();
    if (!model.isUnbound()) {
        chat.model = model.as<const char*>();
    }
    if (!sshProfile.isUnbound()) {
        chat.sshProfile = sshProfile.as<const char*>();
    }
    const bool sshToolsEnabled = document["ssh_tools_enabled"].as<bool>();
    const JsonVariantConst canonicalPolicy =
        document.as<JsonObjectConst>()["tool_policy"];
    if (canonicalPolicy.isUnbound()) {
        chat.toolPolicy = migrateLegacyChatToolPermissionPolicy(sshToolsEnabled);
    } else {
        if (!canonicalPolicy.is<const char*>()) {
            return {false, {}, "Project chat tool_policy must be a string"};
        }
        const char* encodedPolicy = canonicalPolicy.as<const char*>();
        const ScopedToolPermissionPolicyDecodeResult decodedPolicy =
            decodeScopedToolPermissionPolicy(
                encodedPolicy, std::strlen(encodedPolicy));
        if (decodedPolicy.error != ToolPolicyCodecError::None) {
            return {
                false,
                {},
                String("Project chat tool_policy is invalid: ") +
                    toolPolicyCodecErrorText(decodedPolicy.error),
            };
        }
        if (legacySshToolsEnabled(decodedPolicy.policy) != sshToolsEnabled) {
            return {
                false,
                {},
                "Project chat tool_policy disagrees with ssh_tools_enabled compatibility metadata",
            };
        }
        chat.toolPolicy = decodedPolicy.policy;
    }
    chat.summarizedMessageCount = document["summarized_message_count"].as<std::uint32_t>();
    JsonStringFieldResult instructions = readJsonStringField(
        path, "instructions", kMaximumProjectChatInstructionsBytes);
    if (!instructions.success) return {false, {}, instructions.error};
    chat.instructions = std::move(instructions.value);
    JsonStringFieldResult draft = readJsonStringField(
        path, "draft", kMaximumProjectChatDraftBytes);
    if (!draft.success) return {false, {}, draft.error};
    chat.draft = std::move(draft.value);
    JsonStringFieldResult contextSummary = readJsonStringField(
        path, "context_summary", kMaximumProjectChatSummaryBytes);
    if (!contextSummary.success) return {false, {}, contextSummary.error};
    chat.contextSummary = std::move(contextSummary.value);
    if (chat.model.length() > kMaximumProjectChatModelBytes ||
        !isValidUtf8(chat.model.c_str())) {
        return {false, {}, "Project chat model must be valid UTF-8 up to 240 bytes"};
    }
    if (chat.projectId != projectId || chat.summary.id != chatId ||
        !isValidChatId(chat.projectId.c_str()) || !isValidChatId(chat.summary.id.c_str()) ||
        chat.summary.title.isEmpty() || !isValidUtf8(chat.summary.title.c_str()) ||
        !isValidUtf8(chat.instructions) || !isValidUtf8(chat.draft) ||
        !isValidUtf8(chat.contextSummary) ||
        chat.summarizedMessageCount > chat.summary.messageCount) {
        return {false, {}, "Project chat metadata contains invalid values"};
    }
    return {true, std::move(chat), ""};
}

ProjectChatMetadataStateResult readProjectChatMetadataState(
    const String& projectId, const String& chatId)
{
    const String metadataPath = projectChatMetadataPath(projectId, chatId);
    const OperationResult recovered = recoverAtomicSdFile(metadataPath);
    if (!recovered.success) {
        return {false, 0, 0, recovered.error};
    }
    File metadata = SD.open(metadataPath, FILE_READ);
    if (!metadata) {
        return {false, 0, 0, "Project chat metadata does not exist"};
    }
    JsonDocument filter;
    filter["version"] = true;
    filter["project_id"] = true;
    filter["id"] = true;
    filter["message_count"] = true;
    filter["revision"] = true;
    JsonDocument document;
    const DeserializationError error = deserializeJson(
        document, metadata, DeserializationOption::Filter(filter));
    metadata.close();
    if (error) {
        return {false, 0, 0,
                "Project chat metadata state parse failed: " + String(error.c_str())};
    }
    if (!document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != kProjectChatFormatVersion ||
        !document["project_id"].is<const char*>() ||
        !document["id"].is<const char*>() ||
        !document["message_count"].is<std::uint32_t>() ||
        !document["revision"].is<std::uint32_t>() ||
        document["project_id"].as<String>() != projectId ||
        document["id"].as<String>() != chatId) {
        return {false, 0, 0, "Project chat metadata state is invalid"};
    }
    return {
        true,
        document["message_count"].as<std::uint32_t>(),
        document["revision"].as<std::uint32_t>(),
        "",
    };
}

OperationResult writeProjectChatMetadataWithRevision(
    const ChatDocument& chat, std::uint32_t revision)
{
    const OperationResult validation = validateProjectChatMetadataValues(chat);
    if (!validation.success) {
        return validation;
    }
    const ToolPolicyEncodeResult policy =
        encodeScopedToolPermissionPolicy(chat.toolPolicy);
    if (policy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project chat tool policy cannot be stored: ") +
                toolPolicyCodecErrorText(policy.error),
        };
    }
    JsonDocument document;
    document["version"] = kProjectChatFormatVersion;
    document["project_id"] = chat.projectId;
    document["id"] = chat.summary.id;
    document["title"] = chat.summary.title;
    document["updated_at"] = chat.summary.updatedAt;
    document["message_count"] = chat.summary.messageCount;
    document["pinned"] = chat.summary.pinned;
    document["archived"] = chat.summary.archived;
    document["revision"] = revision;
    document["instructions"] = chat.instructions.c_str();
    document["draft"] = chat.draft.c_str();
    document["tool_policy"] = policy.encoded.value.data();
    document["ssh_tools_enabled"] = legacySshToolsEnabled(chat.toolPolicy);
    document["ssh_profile"] = chat.sshProfile;
    document["context_summary"] = chat.contextSummary.c_str();
    document["summarized_message_count"] = chat.summarizedMessageCount;
    document["model"] = chat.model;
    return writeAtomicJsonSdFile(
        projectChatMetadataPath(chat.projectId, chat.summary.id), document);
}

OperationResult writeProjectChatMetadata(const ChatDocument& chat)
{
    return writeProjectChatMetadataWithRevision(chat, chat.summary.revision);
}

OperationResult validateHistoryFile(const String& path, std::uint32_t expectedMessages)
{
    const TailSourceScanResult scanned = scanTailSource(
        path, 0, TailRecordSource::RawHistory);
    if (!scanned.success) {
        return {false, scanned.error};
    }
    const bool exactState = scanned.recordCount == expectedMessages &&
        (expectedMessages == 0 ||
         (scanned.firstSequence == 1 && scanned.lastSequence == expectedMessages));
    return exactState
        ? OperationResult{true, ""}
        : OperationResult{false, "Project chat raw history count does not match metadata"};
}

OperationResult cleanupFailedChatImport(const String& chatDirectory, const String& error)
{
    const OperationResult cleanup = removeSdDirectoryTree(chatDirectory);
    return cleanup.success
        ? OperationResult{false, error}
        : OperationResult{false, error + "; migrated chat cleanup also failed: " +
                                   cleanup.error};
}

OperationResult recoverPendingAppend(const String& projectId, const String& chatId)
{
    const String markerPath = projectChatAppendMarkerPath(projectId, chatId);
    if (!SD.exists(markerPath)) {
        return {true, ""};
    }
    File marker = SD.open(markerPath, FILE_READ);
    if (!marker) {
        return {false, "Pending chat append marker cannot be opened"};
    }
    JsonDocument transaction;
    const DeserializationError markerError = deserializeJson(transaction, marker);
    marker.close();
    if (markerError || !transaction["old_count"].is<std::uint32_t>() ||
        !transaction["new_count"].is<std::uint32_t>() ||
        !transaction["updated_at"].is<std::uint64_t>()) {
        return {false, "Pending chat append marker is invalid"};
    }
    const std::uint32_t oldCount = transaction["old_count"].as<std::uint32_t>();
    const std::uint32_t newCount = transaction["new_count"].as<std::uint32_t>();
    if (newCount <= oldCount) {
        return {false, "Pending chat append marker contains an invalid message range"};
    }
    const String metadataPath = projectChatMetadataPath(projectId, chatId);
    const OperationResult recovered = recoverAtomicSdFile(metadataPath);
    if (!recovered.success) {
        return recovered;
    }
    File metadataFile = SD.open(metadataPath, FILE_READ);
    if (!metadataFile) {
        return {false, "Pending chat append cannot recover because metadata is missing"};
    }
    ChatDocumentResult metadata = parseProjectChatMetadata(
        metadataFile, projectId, chatId, metadataPath);
    metadataFile.close();
    if (!metadata.success) {
        return {false, "Pending chat append metadata is invalid: " + metadata.error};
    }
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    const TailSourceScanResult history = scanTailSource(
        historyPath, 0, TailRecordSource::RawHistory);
    if (!history.success) {
        return {false, "Pending chat append raw history is invalid: " + history.error};
    }
    const bool isOldState = history.recordCount == oldCount &&
        (oldCount == 0 ||
         (history.firstSequence == 1 && history.lastSequence == oldCount));
    if (isOldState) {
        return SD.remove(markerPath)
            ? OperationResult{true, ""}
            : OperationResult{false, "Failed to clear an uncommitted chat append marker"};
    }
    const bool isNewState = history.recordCount == newCount &&
        history.firstSequence == 1 && history.lastSequence == newCount;
    if (!isNewState) {
        return {false,
                "Pending chat append raw history is neither the old nor new complete state"};
    }
    const std::size_t firstRetainedOffset = history.retained.count == 0
        ? history.sourceBytes
        : history.retained.records[history.retained.first].offset;
    OperationResult result = writeTailSuffix(
        historyPath, projectChatTailPath(projectId, chatId), firstRetainedOffset);
    if (!result.success) {
        return result;
    }
    metadata.chat.summary.messageCount = newCount;
    metadata.chat.summary.updatedAt = transaction["updated_at"].as<std::uint64_t>();
    ++metadata.chat.summary.revision;
    result = writeProjectChatMetadata(metadata.chat);
    if (result.success) {
        result = upsertProjectChatSummary(projectId, metadata.chat.summary);
    }
    if (!result.success) {
        return result;
    }
    return SD.remove(markerPath)
        ? OperationResult{true, ""}
        : OperationResult{false, "Recovered chat append but failed to clear its marker"};
}

}  // namespace

ChatDocumentResult createProjectChat(
    const String& projectId,
    const String& title,
    const ScopedToolPermissionPolicy& toolPolicy)
{
    if (!isValidChatId(projectId.c_str()) || title.isEmpty() ||
        !isValidUtf8(title.c_str())) {
        return {false, {}, "Cannot create a project chat with invalid metadata"};
    }
    const ToolPolicyEncodeResult encodedPolicy =
        encodeScopedToolPermissionPolicy(toolPolicy);
    if (encodedPolicy.error != ToolPolicyCodecError::None) {
        return {
            false,
            {},
            String("Cannot create a project chat with an invalid tool policy: ") +
                toolPolicyCodecErrorText(encodedPolicy.error),
        };
    }
    const ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, {}, project.error};
    }
    String chatId;
    for (std::uint8_t attempt = 0; attempt < 8; ++attempt) {
        chatId = generateProjectChatId();
        if (!SD.exists(projectChatDirectoryPath(projectId, chatId))) {
            break;
        }
        chatId.clear();
    }
    if (chatId.isEmpty()) {
        return {false, {}, "Failed to generate a unique project chat id after 8 attempts"};
    }
    const String chatDirectory = projectChatDirectoryPath(projectId, chatId);
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return {false, {}, result.error};
    }
    ChatDocument chat;
    chat.projectId = projectId;
    chat.summary = {chatId, title, 0, 0, false, false, 0, 1};
    chat.toolPolicy = toolPolicy;
    result = writeEmptyAtomicSdFile(projectChatHistoryPath(projectId, chatId));
    if (result.success) {
        result = writeEmptyAtomicSdFile(projectChatTailPath(projectId, chatId));
    }
    if (result.success) {
        result = writeProjectChatMetadata(chat);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, chat.summary);
    }
    if (!result.success) {
        return {false, {}, cleanupFailedChatImport(chatDirectory, result.error).error};
    }
    return {true, std::move(chat), ""};
}

OperationResult saveProjectChatMetadata(const ChatDocument& chat)
{
    const OperationResult validation = validateProjectChatMetadataValues(chat);
    if (!validation.success) {
        return validation;
    }
    const OperationResult recovered = recoverPendingAppend(
        chat.projectId, chat.summary.id);
    if (!recovered.success) {
        return recovered;
    }
    const ProjectChatMetadataStateResult current = readProjectChatMetadataState(
        chat.projectId, chat.summary.id);
    if (!current.success) {
        return {false, current.error};
    }
    if (chat.summary.messageCount != current.messageCount) {
        return {false, "Project chat message count can only change through history operations"};
    }
    ChatSummary updatedSummary = chat.summary;
    updatedSummary.revision = current.revision + 1;
    OperationResult result = writeProjectChatMetadataWithRevision(
        chat, updatedSummary.revision);
    if (result.success) {
        result = upsertProjectChatSummary(chat.projectId, updatedSummary);
    }
    return result;
}

OperationResult appendProjectChatMessages(const String& projectId,
                                          const String& chatId,
                                          const std::vector<Message>& messages,
                                          std::uint64_t updatedAt,
                                          std::uint32_t historyQuotaBytes)
{
    if (messages.empty()) {
        return {false, "Project chat append requires at least one message"};
    }
    if (!isValidProjectChatHistoryQuota(historyQuotaBytes)) {
        return {false, "Project chat history quota must be 0 or at least 2 MiB"};
    }
    ChatDocumentResult current = loadProjectChatMetadata(projectId, chatId);
    if (!current.success) {
        return {false, current.error};
    }
    if (messages.size() > std::numeric_limits<std::uint32_t>::max() ||
        static_cast<std::uint32_t>(messages.size()) >
            std::numeric_limits<std::uint32_t>::max() -
                current.chat.summary.messageCount ||
        current.chat.summary.revision == std::numeric_limits<std::uint32_t>::max()) {
        return {false, "Project chat message count would overflow"};
    }
    const std::uint32_t appendedMessages =
        static_cast<std::uint32_t>(messages.size());
    std::uint64_t appendedBytes = 0;
    for (std::size_t index = 0; index < messages.size(); ++index) {
        const Message& message = messages[index];
        if ((message.role != "user" && message.role != "assistant") ||
            message.content.empty() || message.content.size() > kStoredTailBytes ||
            !isValidUtf8(message.content)) {
            return {false, "Cannot append an invalid project chat message"};
        }
        JsonDocument record;
        record["sequence"] = current.chat.summary.messageCount + index + 1;
        record["role"] = message.role.c_str();
        record["content"] = message.content.c_str();
        const std::size_t measuredRecordBytes = measureJson(record);
        if (measuredRecordBytes == std::numeric_limits<std::size_t>::max()) {
            return {false, "Project chat record serialized size would overflow"};
        }
        const std::uint64_t recordBytes =
            static_cast<std::uint64_t>(measuredRecordBytes) + 1U;
        if (appendedBytes >
            std::numeric_limits<std::uint64_t>::max() - recordBytes) {
            return {false, "Project chat append serialized size would overflow"};
        }
        appendedBytes += recordBytes;
    }
    JsonDocument transaction;
    transaction["old_count"] = current.chat.summary.messageCount;
    transaction["new_count"] =
        current.chat.summary.messageCount + appendedMessages;
    transaction["updated_at"] = updatedAt;
    const std::uint64_t markerBytes = measureJson(transaction);
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    const String tailPath = projectChatTailPath(projectId, chatId);
    const String chatIndexPath = projectChatsDirectoryPath(projectId) + "/index.jsonl";
    const String projectMetadataPath = projectDirectoryPath(projectId) + "/project.json";
    const String projectIndexPath = projectStorageRoot() + "/projects/index.jsonl";
    File historyState = SD.open(historyPath, FILE_READ);
    File tailState = SD.open(tailPath, FILE_READ);
    File projectMetadataState = SD.open(projectMetadataPath, FILE_READ);
    if (!historyState || !tailState || !projectMetadataState) {
        if (historyState) historyState.close();
        if (tailState) tailState.close();
        if (projectMetadataState) projectMetadataState.close();
        return {false, "Project chat append transaction files are unavailable for preflight"};
    }
    const std::uint64_t currentHistoryBytes = historyState.size();
    const std::uint64_t existingTailBytes = tailState.size();
    const std::uint64_t existingProjectMetadataBytes = projectMetadataState.size();
    historyState.close();
    tailState.close();
    projectMetadataState.close();
    if (existingTailBytes >
        std::numeric_limits<std::uint64_t>::max() - appendedBytes) {
        return {false, "Project chat staged tail upper bound would overflow"};
    }
    const std::uint64_t stagedTailBytes = existingTailBytes + appendedBytes;
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    const std::uint32_t newMessageCount =
        current.chat.summary.messageCount + appendedMessages;
    const std::uint32_t newChatRevision = current.chat.summary.revision + 1U;
    const StorageSizeResult stagedChatMetadata = measureUpdatedProjectChatMetadata(
        current.chat, newMessageCount, updatedAt, newChatRevision);
    if (!stagedChatMetadata.success) {
        return {false, stagedChatMetadata.error};
    }
    const std::uint64_t stagedChatMetadataBytes = stagedChatMetadata.bytes;
    const String chatSummaryLine = serializeUpdatedChatSummary(
        current.chat.summary, newMessageCount, updatedAt, newChatRevision);
    const StorageIndexMutationPlanResult chatIndexPlan = planJsonlSdIndexMutation(
        chatIndexPath, "id", chatId, chatSummaryLine, false);
    if (!chatIndexPlan.success || !chatIndexPlan.found) {
        return {false, chatIndexPlan.success
            ? String("Project chat index entry is missing during append preflight")
            : chatIndexPlan.error};
    }
    const ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    if (project.project.summary.revision == std::numeric_limits<std::uint32_t>::max() ||
        project.project.chatIndexRevision == std::numeric_limits<std::uint32_t>::max()) {
        return {false, "Project counters would overflow during chat append"};
    }
    const std::uint32_t newProjectRevision = project.project.summary.revision + 1U;
    const String projectSummaryLine = serializeWorstCaseProjectSummary(
        project.project.summary, newProjectRevision);
    const StorageIndexMutationPlanResult projectIndexPlan = planJsonlSdIndexMutation(
        projectIndexPath, "id", projectId, projectSummaryLine, false);
    if (!projectIndexPlan.success || !projectIndexPlan.found) {
        return {false, projectIndexPlan.success
            ? String("Project index entry is missing during chat append preflight")
            : projectIndexPlan.error};
    }
    const std::uint64_t oldProjectCounterBytes =
        unsignedDecimalBytes(project.project.summary.updatedAt) +
        unsignedDecimalBytes(project.project.summary.chatCount) +
        unsignedDecimalBytes(project.project.summary.revision) +
        unsignedDecimalBytes(project.project.chatIndexRevision);
    const std::uint64_t newProjectCounterBytes =
        unsignedDecimalBytes(std::numeric_limits<std::uint64_t>::max()) +
        unsignedDecimalBytes(project.project.summary.chatCount) +
        unsignedDecimalBytes(newProjectRevision) +
        unsignedDecimalBytes(project.project.chatIndexRevision + 1U);
    if (existingProjectMetadataBytes < oldProjectCounterBytes ||
        existingProjectMetadataBytes - oldProjectCounterBytes >
            maximum - newProjectCounterBytes) {
        return {false, "Project metadata staged size would overflow"};
    }
    const std::uint64_t stagedProjectMetadataBytes =
        existingProjectMetadataBytes - oldProjectCounterBytes + newProjectCounterBytes;
    if (stagedChatMetadataBytes > maximum - stagedProjectMetadataBytes ||
        stagedChatMetadataBytes + stagedProjectMetadataBytes >
            maximum - chatIndexPlan.stagedBytes ||
        stagedChatMetadataBytes + stagedProjectMetadataBytes +
                chatIndexPlan.stagedBytes >
            maximum - projectIndexPlan.stagedBytes) {
        return {false, "Project chat staged metadata transaction size would overflow"};
    }
    const std::uint64_t stagedMetadataBytes = stagedChatMetadataBytes +
        stagedProjectMetadataBytes + chatIndexPlan.stagedBytes +
        projectIndexPlan.stagedBytes;
    const SdStorageStatus storage = inspectSdStorage();
    if (storage.state != SdStorageState::Ready) {
        return {false, storage.error};
    }
    const ProjectChatAppendPlanResult plan = planProjectChatAppend(
        currentHistoryBytes, appendedBytes, historyQuotaBytes, markerBytes,
        stagedTailBytes, stagedMetadataBytes,
        storage.totalBytes - storage.usedBytes,
        kStorageOperationalFloorBytes);
    if (!plan.success) {
        return {false, plan.error};
    }
    OperationResult result = checkSdOperationSpace(
        plan.requiredFreeBytes - kStorageOperationalFloorBytes,
        kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = writeAtomicJsonSdFile(
        projectChatAppendMarkerPath(projectId, chatId), transaction);
    if (!result.success) {
        return {false, "Failed to stage project chat append: " + result.error};
    }
    File historyFile = SD.open(historyPath, FILE_APPEND);
    if (!historyFile) {
        return {false, "Failed to open project chat raw history for append"};
    }
    const std::size_t oldHistoryBytes = historyFile.size();
    if (oldHistoryBytes != currentHistoryBytes) {
        historyFile.close();
        return {false, "Project chat raw history changed after append preflight"};
    }
    for (std::size_t index = 0; index < messages.size() && result.success; ++index) {
        result = writeHistoryMessage(
            historyFile, current.chat.summary.messageCount + index + 1, messages[index]);
    }
    historyFile.flush();
    historyFile.close();
    if (!result.success) {
        return {false, result.error +
                       "; pending append recovery will validate the raw history"};
    }
    result = updateTailAfterAppend(
        projectId, chatId, current.chat.summary.messageCount,
        current.chat.summary.messageCount + appendedMessages,
        oldHistoryBytes);
    if (!result.success) {
        return result;
    }
    ChatDocument updated = std::move(current.chat);
    updated.messages.clear();
    updated.summary.messageCount += appendedMessages;
    updated.summary.updatedAt = updatedAt;
    ++updated.summary.revision;
    result = writeProjectChatMetadata(updated);
    if (result.success) {
        result = upsertProjectChatSummary(projectId, updated.summary);
    }
    if (!result.success) {
        return {false, result.error + "; pending append recovery is required"};
    }
    return SD.remove(projectChatAppendMarkerPath(projectId, chatId))
        ? OperationResult{true, ""}
        : OperationResult{false, "Chat append committed but its recovery marker remains"};
}

OperationResult clearProjectChatHistory(const String& projectId, const String& chatId)
{
    const ChatDocumentResult current = loadProjectChatMetadata(projectId, chatId);
    if (!current.success) {
        return {false, current.error};
    }
    OperationResult result = writeEmptyAtomicSdFile(projectChatHistoryPath(projectId, chatId));
    if (result.success) {
        result = writeEmptyAtomicSdFile(projectChatTailPath(projectId, chatId));
    }
    ChatDocument updated = current.chat;
    updated.messages.clear();
    updated.summary.messageCount = 0;
    updated.summary.archivedMessageCount = 0;
    updated.contextSummary.clear();
    updated.summarizedMessageCount = 0;
    ++updated.summary.revision;
    if (result.success) {
        result = writeProjectChatMetadata(updated);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, updated.summary);
    }
    return result;
}

OperationResult deleteProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult current = loadProjectChatMetadata(projectId, chatId);
    if (!current.success) {
        return {false, current.error};
    }
    OperationResult result = removeProjectChatSummary(projectId, chatId);
    if (!result.success) {
        return result;
    }
    result = removeSdDirectoryTree(projectChatDirectoryPath(projectId, chatId));
    if (!result.success) {
        const OperationResult rollback = upsertProjectChatSummary(projectId, current.chat.summary);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; chat index rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

ChatDocumentResult duplicateProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult source = loadProjectChatMetadata(projectId, chatId);
    if (!source.success) {
        return source;
    }
    String duplicateId;
    for (std::uint8_t attempt = 0; attempt < 8; ++attempt) {
        duplicateId = generateProjectChatId();
        if (!SD.exists(projectChatDirectoryPath(projectId, duplicateId))) {
            break;
        }
        duplicateId.clear();
    }
    if (duplicateId.isEmpty()) {
        return {false, {}, "Failed to generate a unique duplicate chat id after 8 attempts"};
    }
    const String directory = projectChatDirectoryPath(projectId, duplicateId);
    OperationResult result = ensureSdDirectory(directory);
    if (result.success) {
        result = copySdFileAtomically(
            projectChatHistoryPath(projectId, chatId),
            projectChatHistoryPath(projectId, duplicateId),
            kStorageOperationalFloorBytes);
    }
    if (result.success) {
        result = copySdFileAtomically(
            projectChatTailPath(projectId, chatId),
            projectChatTailPath(projectId, duplicateId),
            kStorageOperationalFloorBytes);
    }
    ChatDocument duplicate = source.chat;
    duplicate.summary.id = duplicateId;
    duplicate.summary.title += " copy";
    duplicate.summary.revision = 1;
    duplicate.messages.clear();
    if (result.success) {
        result = writeProjectChatMetadata(duplicate);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, duplicate.summary);
    }
    if (!result.success) {
        return {false, {}, cleanupFailedChatImport(directory, result.error).error};
    }
    return loadProjectChat(projectId, duplicateId, 64, 65536);
}

ArchivedMessagesPageResult readProjectChatMessages(const String& projectId,
                                                   const String& chatId,
                                                   std::uint32_t offset,
                                                   std::size_t maximumMessages,
                                                   std::size_t maximumBytes)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str()) ||
        maximumMessages == 0 || maximumBytes == 0) {
        return {false, {}, offset, true, "Project chat page arguments are invalid"};
    }
    const OperationResult recovered = recoverPendingAppend(projectId, chatId);
    if (!recovered.success) {
        return {false, {}, offset, true, recovered.error};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, {}, offset, true, "Project chat raw history does not exist"};
    }
    const std::size_t sourceBytes = history.size();
    if (sourceBytes > UINT32_MAX || offset > sourceBytes || !history.seek(offset)) {
        history.close();
        return {false, {}, offset, true, "Project chat page offset is outside raw history"};
    }
    BufferedFileReader reader(history);
    std::vector<Message> messages;
    try {
        messages.reserve(maximumMessages);
    } catch (const std::bad_alloc&) {
        history.close();
        return {false, {}, offset, true,
                "Failed to allocate the project chat page"};
    }
    JsonDocument headerFilter;
    headerFilter["sequence"] = true;
    headerFilter["role"] = true;
    std::size_t bytes = 0;
    std::uint32_t previousSequence = 0;
    while (messages.size() < maximumMessages && reader.prepareRecord()) {
        const std::size_t recordPosition = reader.position();
        JsonDocument header;
        const DeserializationError headerError = deserializeJson(
            header, reader, DeserializationOption::Filter(headerFilter));
        const bool validHeaderTypes = !headerError &&
            header["sequence"].is<std::uint32_t>() &&
            header["role"].is<const char*>();
        const std::uint32_t sequence = validHeaderTypes
            ? header["sequence"].as<std::uint32_t>() : 0;
        const char* roleValue = validHeaderTypes
            ? header["role"].as<const char*>() : nullptr;
        const String role = roleValue == nullptr ? String() : String(roleValue);
        const bool validSequence = sequence != 0 &&
            (previousSequence == 0
                ? (offset != 0 || sequence == 1)
                : (previousSequence != UINT32_MAX &&
                   sequence == previousSequence + 1));
        if (reader.failed() || reader.recordLimitExceeded() ||
            !validHeaderTypes || !validSequence ||
            (role != "user" && role != "assistant")) {
            history.close();
            return {false, {}, offset, true, "Project chat page contains an invalid record"};
        }
        if (!reader.seek(recordPosition)) {
            history.close();
            return {false, {}, offset, true,
                    "Failed to rewind the project chat page record"};
        }
        json_reader::JsonStringValueResult content =
            json_reader::readObjectStringField(
                reader, "content", 64, kStoredTailBytes);
        if (!content.success || reader.failed() ||
            reader.recordLimitExceeded() || !reader.consumeRecordDelimiter() ||
            content.value.empty() || !isValidUtf8(content.value)) {
            history.close();
            return {false, {}, offset, true,
                    "Project chat page content or delimiter is invalid"};
        }
        if (!messages.empty() &&
            (bytes > maximumBytes ||
             content.value.size() > maximumBytes - bytes)) {
            if (!reader.seek(recordPosition)) {
                history.close();
                return {false, {}, offset, true,
                        "Failed to restore the project chat page cursor"};
            }
            const std::uint32_t nextOffset =
                static_cast<std::uint32_t>(recordPosition);
            history.close();
            return {true, std::move(messages), nextOffset, false, ""};
        }
        bytes += content.value.size();
        messages.push_back({role, std::move(content.value)});
        previousSequence = sequence;
    }
    if (reader.failed() || reader.recordLimitExceeded()) {
        history.close();
        return {false, {}, offset, true, "Failed to read the project chat page"};
    }
    const std::size_t logicalPosition = reader.position();
    if (logicalPosition > UINT32_MAX) {
        history.close();
        return {false, {}, offset, true,
                "Project chat page cursor exceeds the supported range"};
    }
    const std::uint32_t nextOffset = static_cast<std::uint32_t>(logicalPosition);
    const bool eof = logicalPosition >= sourceBytes;
    history.close();
    return {true, std::move(messages), nextOffset, eof, ""};
}

IndexedMessagesPageResult readProjectChatMessagesByIndex(
    const String& projectId,
    const String& chatId,
    std::uint32_t firstMessageIndex,
    std::size_t maximumMessages,
    std::size_t maximumBytes)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str()) ||
        maximumMessages == 0 || maximumBytes == 0) {
        return {false, {}, firstMessageIndex, true,
                "Project chat indexed page arguments are invalid"};
    }
    const OperationResult recovered = recoverPendingAppend(projectId, chatId);
    if (!recovered.success) {
        return {false, {}, firstMessageIndex, true, recovered.error};
    }
    const ProjectChatMetadataStateResult metadata = readProjectChatMetadataState(
        projectId, chatId);
    if (!metadata.success) {
        return {false, {}, firstMessageIndex, true, metadata.error};
    }
    if (firstMessageIndex > metadata.messageCount) {
        return {false, {}, firstMessageIndex, true,
                "Project chat message index is outside raw history"};
    }
    if (firstMessageIndex == metadata.messageCount) {
        return {true, {}, firstMessageIndex, true, ""};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, {}, firstMessageIndex, true,
                "Project chat raw history does not exist"};
    }
    BufferedFileReader reader(history);
    std::vector<Message> messages;
    try {
        messages.reserve(maximumMessages);
    } catch (const std::bad_alloc&) {
        history.close();
        return {false, {}, firstMessageIndex, true,
                "Failed to allocate the project chat indexed page"};
    }
    JsonDocument headerFilter;
    headerFilter["sequence"] = true;
    headerFilter["role"] = true;
    std::uint32_t messageIndex = 0;
    std::size_t contentBytes = 0;
    while (reader.prepareRecord()) {
        if (messageIndex >= firstMessageIndex && messages.size() >= maximumMessages) {
            history.close();
            return {true, std::move(messages), messageIndex, false, ""};
        }
        const std::size_t recordPosition = reader.position();
        JsonDocument header;
        const DeserializationError headerError = deserializeJson(
            header, reader, DeserializationOption::Filter(headerFilter));
        const std::uint32_t sequence = headerError
            ? 0 : header["sequence"].as<std::uint32_t>();
        const char* roleValue = headerError
            ? nullptr : header["role"].as<const char*>();
        const String role = roleValue == nullptr ? String() : String(roleValue);
        if (reader.failed() || reader.recordLimitExceeded() || headerError ||
            sequence != messageIndex + 1 ||
            (role != "user" && role != "assistant")) {
            history.close();
            return {false, {}, firstMessageIndex, true,
                    "Project chat indexed page contains an invalid record"};
        }
        if (messageIndex < firstMessageIndex) {
            if (!reader.consumeRecordDelimiter()) {
                history.close();
                return {false, {}, firstMessageIndex, true,
                        "Project chat indexed page prefix has an invalid delimiter"};
            }
            ++messageIndex;
            continue;
        }
        if (!reader.seek(recordPosition)) {
            history.close();
            return {false, {}, firstMessageIndex, true,
                    "Failed to rewind the project chat indexed record"};
        }
        json_reader::JsonStringValueResult content =
            json_reader::readObjectStringField(
                reader, "content", 64, kStoredTailBytes);
        if (!content.success || !reader.consumeRecordDelimiter()) {
            history.close();
            return {false, {}, firstMessageIndex, true,
                    "Project chat indexed page content is invalid"};
        }
        if (contentBytes + content.value.size() > maximumBytes && !messages.empty()) {
            history.close();
            return {true, std::move(messages), messageIndex, false, ""};
        }
        contentBytes += content.value.size();
        messages.push_back({role, std::move(content.value)});
        ++messageIndex;
    }
    const bool validEnd = !reader.failed() &&
        messageIndex == metadata.messageCount;
    history.close();
    if (!validEnd) {
        return {false, {}, firstMessageIndex, true,
                "Project chat indexed page ended before metadata count"};
    }
    return {true, std::move(messages), messageIndex, true, ""};
}

OperationResult exportProjectChatMarkdown(const String& projectId,
                                          const String& chatId,
                                          const String& filename)
{
    if (!isValidWorkspaceFilename(filename.c_str())) {
        return {false, "Project chat export filename is invalid"};
    }
    const ChatDocumentResult chat = loadProjectChatMetadata(projectId, chatId);
    if (!chat.success) {
        return {false, chat.error};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, "Project chat raw history does not exist"};
    }
    const String target = workspaceFilePath(filename);
    OperationResult result = recoverAtomicSdFile(target);
    const String stagedPath = target + ".tmp";
    File output;
    if (result.success) {
        result = checkSdOperationSpace(history.size(), kStorageOperationalFloorBytes);
    }
    if (result.success) {
        output = SD.open(stagedPath, FILE_WRITE);
        if (!output) {
            result = {false, "Failed to create staged Markdown chat export"};
        }
    }
    if (result.success) {
        const String titleLine = "# " + chat.chat.summary.title;
        if (output.print(titleLine) != titleLine.length() || output.write('\n') != 1 ||
            output.write('\n') != 1) {
            result = {false, "Failed to write complete staged Markdown chat export"};
        }
    }
    while (history.available() && result.success) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument record;
        const DeserializationError error = deserializeJson(record, line);
        if (error || !record["role"].is<const char*>() ||
            !record["content"].is<const char*>()) {
            result = {false, "Project chat history is invalid during Markdown export"};
            break;
        }
        const String roleLine = String("## ") +
            (String(record["role"].as<const char*>()) == "user" ? "You" : "AI");
        const char* content = record["content"].as<const char*>();
        const std::size_t contentBytes = std::strlen(content);
        if (output.print(roleLine) != roleLine.length() || output.write('\n') != 1 ||
            output.write('\n') != 1 ||
            output.write(reinterpret_cast<const std::uint8_t*>(content), contentBytes) !=
                contentBytes ||
            output.write('\n') != 1 || output.write('\n') != 1) {
            result = {false, "Failed to write complete staged Markdown chat export"};
        }
    }
    history.close();
    if (output) {
        output.flush();
        output.close();
    }
    if (!result.success) {
        if (SD.exists(stagedPath) && !SD.remove(stagedPath)) {
            result.error += "; failed to remove staged Markdown chat export";
        }
        return result;
    }
    return commitStagedSdFile(target, stagedPath);
}

OperationResult importLegacyChatToProject(const String& projectId,
                                          const ChatDocument& legacyChat)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(legacyChat.summary.id.c_str())) {
        return {false, "Cannot migrate an invalid project or chat id"};
    }
    const String chatDirectory = projectChatDirectoryPath(projectId, legacyChat.summary.id);
    if (SD.exists(chatDirectory)) {
        return {false, "Project chat directory already exists during migration"};
    }
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return result;
    }
    const String historyPath = projectChatHistoryPath(projectId, legacyChat.summary.id);
    const String stagedHistoryPath = historyPath + ".tmp";
    const LegacyHistoryMeasurement measurement = measureLegacyHistory(legacyChat);
    if (!measurement.success) {
        return cleanupFailedChatImport(chatDirectory, measurement.error);
    }
    const std::uint32_t expectedMessages = legacyChat.summary.archivedMessageCount +
        static_cast<std::uint32_t>(legacyChat.messages.size());
    if (measurement.messages != expectedMessages) {
        return cleanupFailedChatImport(
            chatDirectory, "Legacy archived message count does not match raw history");
    }
    result = checkSdOperationSpace(measurement.bytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return cleanupFailedChatImport(chatDirectory, result.error);
    }
    File staged = SD.open(stagedHistoryPath, FILE_WRITE);
    if (!staged) {
        return cleanupFailedChatImport(chatDirectory,
                                       "Failed to create staged raw chat history");
    }
    std::uint32_t sequence = 1;
    std::uint32_t archiveOffset = 0;
    bool archiveEof = false;
    while (!archiveEof && result.success) {
        const ArchivedMessagesPageResult page = readArchivedChatMessages(
            legacyChat.summary.id, archiveOffset, 16, 32768);
        if (!page.success) {
            result = {false, page.error};
            break;
        }
        for (const Message& message : page.messages) {
            result = writeHistoryMessage(staged, sequence, message);
            if (!result.success) {
                break;
            }
            ++sequence;
        }
        archiveOffset = page.nextOffset;
        archiveEof = page.eof;
    }
    for (const Message& message : legacyChat.messages) {
        if (!result.success) {
            break;
        }
        result = writeHistoryMessage(staged, sequence, message);
        if (result.success) {
            ++sequence;
        }
    }
    staged.flush();
    staged.close();
    const std::uint32_t totalMessages = sequence - 1;
    if (result.success && totalMessages != expectedMessages) {
        result = {false, "Legacy archived message count does not match raw history"};
    }
    if (result.success) {
        result = validateHistoryFile(stagedHistoryPath, totalMessages);
    }
    if (result.success) {
        result = commitStagedSdFile(historyPath, stagedHistoryPath);
    }
    ChatDocument migrated = legacyChat;
    migrated.projectId = projectId;
    migrated.messages.clear();
    migrated.summary.messageCount = totalMessages;
    migrated.summary.archivedMessageCount = 0;
    migrated.summary.revision = 1;
    migrated.contextSummary.clear();
    migrated.summarizedMessageCount = 0;
    if (result.success) {
        result = rebuildTailFile(projectId, legacyChat.summary.id, totalMessages);
    }
    if (result.success) {
        result = writeProjectChatMetadata(migrated);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, migrated.summary);
    }
    if (!result.success) {
        return cleanupFailedChatImport(chatDirectory, result.error);
    }
    return {true, ""};
}

namespace {

ChatDocumentResult loadProjectChatMetadataDocument(const String& projectId,
                                                   const String& chatId)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str())) {
        return {false, {}, "Cannot load project chat with an invalid id"};
    }
    const String metadataPath = projectChatMetadataPath(projectId, chatId);
    OperationResult recovered = recoverAtomicSdFile(metadataPath);
    if (recovered.success) {
        recovered = recoverPendingAppend(projectId, chatId);
    }
    if (!recovered.success) {
        return {false, {}, recovered.error};
    }
    File metadata = SD.open(metadataPath, FILE_READ);
    if (!metadata) {
        return {false, {}, "Project chat metadata does not exist"};
    }
    ChatDocumentResult result = parseProjectChatMetadata(
        metadata, projectId, chatId, metadataPath);
    metadata.close();
    if (!result.success) {
        return result;
    }
    if (!SD.exists(projectChatTailPath(projectId, chatId))) {
        const OperationResult rebuilt = rebuildTailFile(
            projectId, chatId, result.chat.summary.messageCount);
        if (!rebuilt.success) {
            return {false, {}, rebuilt.error};
        }
    }
    result.chat.messages.clear();
    return result;
}

TailSourceScanResult scanProjectChatTail(const String& projectId,
                                         const String& chatId,
                                         std::uint32_t messageCount)
{
    TailSourceScanResult scanned = scanTailSource(
        projectChatTailPath(projectId, chatId), 0, TailRecordSource::ExistingTail);
    const bool exactMetadata = scanned.success && scanned.recordCount <= messageCount &&
        ((messageCount == 0 && scanned.recordCount == 0) ||
         (messageCount > 0 && scanned.recordCount > 0 &&
          scanned.lastSequence == messageCount &&
          scanned.firstSequence == messageCount - scanned.recordCount + 1));
    if (!exactMetadata) {
        scanned.success = false;
        if (scanned.error.isEmpty()) {
            scanned.error = "Project chat context tail does not match metadata";
        }
    }
    return scanned;
}

}  // namespace

ChatDocumentResult loadProjectChatMetadata(const String& projectId,
                                           const String& chatId)
{
    ChatDocumentResult result = loadProjectChatMetadataDocument(projectId, chatId);
    if (!result.success) {
        return result;
    }
    const TailSourceScanResult scanned = scanProjectChatTail(
        projectId, chatId, result.chat.summary.messageCount);
    if (!scanned.success) {
        return {false, {}, "Project chat context tail is invalid: " + scanned.error};
    }
    return result;
}

ChatDocumentResult loadProjectChat(const String& projectId,
                                   const String& chatId,
                                   std::size_t maximumTailMessages,
                                   std::size_t maximumTailBytes)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str())) {
        return {false, {}, "Cannot load project chat with an invalid id"};
    }
    if (maximumTailMessages == 0 || maximumTailBytes == 0) {
        return {false, {}, "Project chat tail limits must be greater than zero"};
    }
    ChatDocumentResult result = loadProjectChatMetadataDocument(projectId, chatId);
    if (!result.success) {
        return result;
    }
    const String tailPath = projectChatTailPath(projectId, chatId);
    const TailSourceScanResult scanned = scanProjectChatTail(
        projectId, chatId, result.chat.summary.messageCount);
    if (!scanned.success) {
        return {false, {}, "Project chat context tail is invalid: " + scanned.error};
    }
    const std::size_t retainedMessageCapacity =
        maximumTailMessages < kStoredTailMessages
            ? maximumTailMessages
            : kStoredTailMessages;
    std::size_t selectedCount = 0;
    std::size_t selectedBytes = 0;
    std::size_t selectedFirst = scanned.retained.first;
    while (selectedCount < scanned.retained.count &&
           selectedCount < retainedMessageCapacity) {
        const std::size_t slot =
            (scanned.retained.first + scanned.retained.count - selectedCount - 1) %
            kStoredTailMessages;
        const TailRecordSpan& candidate = scanned.retained.records[slot];
        if (selectedCount > 0 &&
            (selectedBytes > maximumTailBytes ||
             candidate.contentBytes > maximumTailBytes - selectedBytes)) {
            break;
        }
        selectedBytes += candidate.contentBytes;
        selectedFirst = slot;
        ++selectedCount;
    }
    if (selectedCount == 0) {
        result.chat.messages.clear();
        return result;
    }
    File history = SD.open(tailPath, FILE_READ);
    if (!history || history.size() != scanned.sourceBytes ||
        !history.seek(scanned.retained.records[selectedFirst].offset)) {
        if (history) history.close();
        return {false, {}, "Failed to open the selected project chat context tail"};
    }
    BufferedFileReader reader(history);
    std::vector<Message> tail;
    try {
        tail.reserve(selectedCount);
    } catch (const std::bad_alloc&) {
        history.close();
        return {false, {}, "Failed to allocate the project chat tail message list"};
    }
    JsonDocument headerFilter;
    headerFilter["sequence"] = true;
    headerFilter["role"] = true;
    for (std::size_t selectedIndex = 0;
         selectedIndex < selectedCount; ++selectedIndex) {
        if (!reader.prepareRecord()) {
            history.close();
            return {false, {}, "Project chat context tail ended before its selected suffix"};
        }
        const std::size_t recordPosition = reader.position();
        const std::size_t selectedSlot =
            (selectedFirst + selectedIndex) % kStoredTailMessages;
        const TailRecordSpan& expectedSpan = scanned.retained.records[selectedSlot];
        std::uint32_t sequence = 0;
        String role;
        DeserializationError error;
        {
            JsonDocument header;
            error = deserializeJson(
                header, reader, DeserializationOption::Filter(headerFilter));
            if (!error && header["sequence"].is<std::uint32_t>() &&
                header["role"].is<const char*>()) {
                sequence = header["sequence"].as<std::uint32_t>();
                role = header["role"].as<const char*>();
            }
        }
        if (reader.failed()) {
            history.close();
            return {false, {}, "Failed to read the project chat context tail"};
        }
        if (reader.recordLimitExceeded()) {
            history.close();
            return {false, {}, "Project chat context tail record exceeds its serialized limit"};
        }
        if (error) {
            history.close();
            return {false, {},
                    "Project chat context tail header parse failed: " +
                        String(error.c_str())};
        }
        if (sequence == 0 || role.isEmpty()) {
            history.close();
            return {false, {}, "Project chat raw history contains an invalid typed record"};
        }
        const std::uint32_t expectedSequence =
            scanned.lastSequence - static_cast<std::uint32_t>(selectedCount) + 1 +
            static_cast<std::uint32_t>(selectedIndex);
        if (recordPosition != expectedSpan.offset || sequence != expectedSequence ||
            (role != "user" && role != "assistant")) {
            history.close();
            return {false, {}, "Project chat raw history sequence or message is invalid"};
        }
        if (!reader.seek(recordPosition)) {
            history.close();
            return {false, {}, "Failed to rewind the project chat context tail record"};
        }
        json_reader::JsonStringValueResult content =
            json_reader::readObjectStringField(
                reader, "content", 64, kStoredTailBytes);
        if (!content.success) {
            history.close();
            if (reader.failed()) {
                return {false, {}, "Failed to read the project chat context tail content"};
            }
            if (reader.recordLimitExceeded()) {
                return {false, {},
                        "Project chat context tail record exceeds its serialized limit"};
            }
            return {false, {}, "Project chat context tail content is invalid: " +
                                       content.error};
        }
        if (!reader.consumeRecordDelimiter()) {
            history.close();
            if (reader.failed()) {
                return {false, {}, "Failed to read the project chat context tail delimiter"};
            }
            if (reader.recordLimitExceeded()) {
                return {false, {},
                        "Project chat context tail record exceeds its serialized limit"};
            }
            return {false, {}, "Project chat context tail record has an invalid delimiter"};
        }
        Message message = {role, std::move(content.value)};
        if (message.content.size() != expectedSpan.contentBytes ||
            message.content.empty() || !isValidUtf8(message.content)) {
            history.close();
            return {false, {}, "Project chat raw history sequence or message is invalid"};
        }
        tail.push_back(std::move(message));
    }
    if (reader.failed() || reader.position() != scanned.sourceBytes) {
        history.close();
        return {false, {}, "Failed to finish reading the project chat context tail"};
    }
    history.close();
    result.chat.messages = std::move(tail);
    return result;
}

OperationResult validateProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult loaded = loadProjectChatMetadata(projectId, chatId);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    return validateHistoryFile(projectChatHistoryPath(projectId, chatId),
                               loaded.chat.summary.messageCount);
}

OperationResult cloneProjectChat(const String& sourceProjectId,
                                 const String& destinationProjectId,
                                 const String& chatId)
{
    if (!isValidChatId(sourceProjectId.c_str()) ||
        !isValidChatId(destinationProjectId.c_str()) || !isValidChatId(chatId.c_str()) ||
        sourceProjectId == destinationProjectId) {
        return {false, "Cannot clone chat with invalid or identical project ids"};
    }
    const ChatDocumentResult source = loadProjectChatMetadata(
        sourceProjectId, chatId);
    if (!source.success) {
        return {false, source.error};
    }
    const String destinationDirectory = projectChatDirectoryPath(destinationProjectId, chatId);
    if (SD.exists(destinationDirectory)) {
        return {false, "Destination project already contains this chat id"};
    }
    OperationResult result = ensureSdDirectory(destinationDirectory);
    if (!result.success) {
        return result;
    }
    result = copySdFileAtomically(
        projectChatHistoryPath(sourceProjectId, chatId),
        projectChatHistoryPath(destinationProjectId, chatId),
        kStorageOperationalFloorBytes);
    if (result.success) {
        result = copySdFileAtomically(
            projectChatTailPath(sourceProjectId, chatId),
            projectChatTailPath(destinationProjectId, chatId),
            kStorageOperationalFloorBytes);
    }
    ChatDocument cloned = source.chat;
    cloned.projectId = destinationProjectId;
    cloned.messages.clear();
    cloned.summary.revision = 1;
    if (result.success) {
        result = writeProjectChatMetadata(cloned);
    }
    if (result.success) {
        result = upsertProjectChatSummary(destinationProjectId, cloned.summary);
    }
    return result.success ? OperationResult{true, ""}
                          : cleanupFailedChatImport(destinationDirectory, result.error);
}

OperationResult writeProjectChatBundleRecords(File& output,
                                              const String& projectId,
                                              const String& chatId)
{
    if (!output) {
        return {false, "Project bundle output is not open"};
    }
    const ChatDocumentResult loaded = loadProjectChatMetadata(projectId, chatId);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const ToolPolicyEncodeResult policy =
        encodeScopedToolPermissionPolicy(loaded.chat.toolPolicy);
    if (policy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project chat tool policy cannot be exported: ") +
                toolPolicyCodecErrorText(policy.error),
        };
    }
    JsonDocument header;
    header["record"] = "chat";
    header["id"] = loaded.chat.summary.id;
    header["title"] = loaded.chat.summary.title;
    header["updated_at"] = loaded.chat.summary.updatedAt;
    header["message_count"] = loaded.chat.summary.messageCount;
    header["pinned"] = loaded.chat.summary.pinned;
    header["archived"] = loaded.chat.summary.archived;
    header["instructions"] = loaded.chat.instructions;
    header["draft"] = loaded.chat.draft;
    header["tool_policy"] = policy.encoded.value.data();
    header["ssh_tools_enabled"] = legacySshToolsEnabled(loaded.chat.toolPolicy);
    header["context_summary"] = loaded.chat.contextSummary;
    header["summarized_message_count"] = loaded.chat.summarizedMessageCount;
    header["model"] = loaded.chat.model;
    const std::size_t headerBytes = measureJson(header);
    if (serializeJson(header, output) != headerBytes || output.write('\n') != 1) {
        return {false, "Failed to write project bundle chat header"};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, "Project chat raw history does not exist"};
    }
    std::uint32_t writtenMessages = 0;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument source;
        const DeserializationError error = deserializeJson(source, line);
        if (error || !source["sequence"].is<std::uint32_t>() ||
            !source["role"].is<const char*>() || !source["content"].is<const char*>()) {
            history.close();
            return {false, "Project chat raw history is invalid during export"};
        }
        JsonDocument record;
        record["record"] = "message";
        record["chat_id"] = chatId;
        record["sequence"] = source["sequence"].as<std::uint32_t>();
        record["role"] = source["role"].as<const char*>();
        record["content"] = source["content"].as<const char*>();
        const std::size_t recordBytes = measureJson(record);
        if (serializeJson(record, output) != recordBytes || output.write('\n') != 1) {
            history.close();
            return {false, "Failed to write project bundle message"};
        }
        ++writtenMessages;
    }
    history.close();
    return writtenMessages == loaded.chat.summary.messageCount
        ? OperationResult{true, ""}
        : OperationResult{false, "Project bundle message count does not match chat metadata"};
}

StorageSizeResult measureProjectChatBundleRecords(const String& projectId,
                                                  const String& chatId)
{
    const ChatDocumentResult loaded = loadProjectChatMetadata(projectId, chatId);
    if (!loaded.success) {
        return {false, 0, loaded.error};
    }
    const ToolPolicyEncodeResult policy =
        encodeScopedToolPermissionPolicy(loaded.chat.toolPolicy);
    if (policy.error != ToolPolicyCodecError::None) {
        return {
            false,
            0,
            String("Project chat tool policy cannot be measured for export: ") +
                toolPolicyCodecErrorText(policy.error),
        };
    }
    JsonDocument header;
    header["record"] = "chat";
    header["id"] = loaded.chat.summary.id;
    header["title"] = loaded.chat.summary.title;
    header["updated_at"] = loaded.chat.summary.updatedAt;
    header["message_count"] = loaded.chat.summary.messageCount;
    header["pinned"] = loaded.chat.summary.pinned;
    header["archived"] = loaded.chat.summary.archived;
    header["instructions"] = loaded.chat.instructions;
    header["draft"] = loaded.chat.draft;
    header["tool_policy"] = policy.encoded.value.data();
    header["ssh_tools_enabled"] = legacySshToolsEnabled(loaded.chat.toolPolicy);
    header["context_summary"] = loaded.chat.contextSummary;
    header["summarized_message_count"] = loaded.chat.summarizedMessageCount;
    header["model"] = loaded.chat.model;
    std::uint64_t bytes = measureJson(header) + 1;
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, 0, "Project chat raw history does not exist"};
    }
    std::uint32_t measuredMessages = 0;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument source;
        const DeserializationError error = deserializeJson(source, line);
        if (error || !source["sequence"].is<std::uint32_t>() ||
            !source["role"].is<const char*>() || !source["content"].is<const char*>()) {
            history.close();
            return {false, 0, "Project chat raw history is invalid during measurement"};
        }
        JsonDocument record;
        record["record"] = "message";
        record["chat_id"] = chatId;
        record["sequence"] = source["sequence"].as<std::uint32_t>();
        record["role"] = source["role"].as<const char*>();
        record["content"] = source["content"].as<const char*>();
        bytes += measureJson(record) + 1;
        ++measuredMessages;
    }
    history.close();
    return measuredMessages == loaded.chat.summary.messageCount
        ? StorageSizeResult{true, bytes, ""}
        : StorageSizeResult{false, 0,
                            "Project chat message count changed during bundle measurement"};
}

OperationResult importProjectChatBundleRecords(File& input,
                                               const String& destinationProjectId,
                                               const ChatDocument& metadata,
                                               std::uint32_t messageCount)
{
    if (!input || !isValidChatId(destinationProjectId.c_str()) ||
        !isValidChatId(metadata.summary.id.c_str()) ||
        metadata.summary.messageCount != messageCount || metadata.summary.title.isEmpty() ||
        !isValidUtf8(metadata.summary.title.c_str()) ||
        metadata.instructions.size() > kMaximumProjectChatInstructionsBytes ||
        metadata.draft.size() > kMaximumProjectChatDraftBytes ||
        metadata.contextSummary.size() > kMaximumProjectChatSummaryBytes ||
        metadata.model.length() > kMaximumProjectChatModelBytes ||
        !isValidUtf8(metadata.instructions) || !isValidUtf8(metadata.draft) ||
        !isValidUtf8(metadata.contextSummary) ||
        !isValidUtf8(metadata.model.c_str()) ||
        metadata.summarizedMessageCount > messageCount) {
        return {false, "Project bundle chat import arguments are invalid"};
    }
    const ScopedToolPermissionPolicy importedToolPolicy =
        setLegacySshToolsEnabled(metadata.toolPolicy, false);
    const ToolPolicyEncodeResult encodedPolicy =
        encodeScopedToolPermissionPolicy(importedToolPolicy);
    if (encodedPolicy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project bundle chat tool policy is invalid: ") +
                toolPolicyCodecErrorText(encodedPolicy.error),
        };
    }
    const String chatDirectory = projectChatDirectoryPath(
        destinationProjectId, metadata.summary.id);
    if (SD.exists(chatDirectory)) {
        return {false, "Imported project contains a duplicate chat id"};
    }
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return result;
    }
    const String historyPath = projectChatHistoryPath(
        destinationProjectId, metadata.summary.id);
    const String stagedHistoryPath = historyPath + ".tmp";
    File staged = SD.open(stagedHistoryPath, FILE_WRITE);
    if (!staged) {
        return cleanupFailedChatImport(chatDirectory,
                                       "Failed to stage imported project chat history");
    }
    BufferedFileReader reader(input);
    JsonDocument headerFilter;
    headerFilter["record"] = true;
    headerFilter["chat_id"] = true;
    headerFilter["sequence"] = true;
    headerFilter["role"] = true;
    for (std::uint32_t expectedSequence = 1;
         expectedSequence <= messageCount && result.success; ++expectedSequence) {
        if (!reader.prepareRecord()) {
            result = {false, "Project bundle ended before all chat messages were read"};
            break;
        }
        const std::size_t recordPosition = reader.position();
        String role;
        {
            JsonDocument header;
            const DeserializationError error = deserializeJson(
                header, reader, DeserializationOption::Filter(headerFilter));
            if (reader.recordLimitExceeded()) {
                result = {false,
                          "Project bundle chat message exceeds the 786560-byte record limit"};
                break;
            }
            if (reader.failed()) {
                result = {false, "Failed to read the project bundle chat message header"};
                break;
            }
            const char* recordType = error ? nullptr : header["record"].as<const char*>();
            const char* sourceChatId = error ? nullptr : header["chat_id"].as<const char*>();
            const char* roleValue = error ? nullptr : header["role"].as<const char*>();
            if (error || recordType == nullptr ||
                std::strcmp(recordType, "message") != 0 || sourceChatId == nullptr ||
                metadata.summary.id != sourceChatId ||
                !header["sequence"].is<std::uint32_t>() ||
                header["sequence"].as<std::uint32_t>() != expectedSequence ||
                roleValue == nullptr ||
                (std::strcmp(roleValue, "user") != 0 &&
                 std::strcmp(roleValue, "assistant") != 0)) {
                result = {false, "Project bundle contains an invalid chat message record"};
                break;
            }
            role = roleValue;
        }
        if (!reader.seek(recordPosition)) {
            result = {false, "Failed to rewind the project bundle chat message"};
            break;
        }
        json_reader::JsonStringValueResult content =
            json_reader::readObjectStringField(
                reader, "content", 64, kStoredTailBytes);
        if (!content.success) {
            if (reader.failed()) {
                result = {false, "Failed to read the project bundle chat message content"};
            } else if (reader.recordLimitExceeded()) {
                result = {false,
                          "Project bundle chat message exceeds the 786560-byte record limit"};
            } else {
                result = {false, "Project bundle chat message content is invalid: " +
                                     content.error};
            }
            break;
        }
        if (content.value.empty() || !isValidUtf8(content.value)) {
            result = {false, "Project bundle chat message content is invalid"};
            break;
        }
        if (!reader.consumeRecordDelimiter()) {
            result = {false, reader.failed()
                ? String("Failed to read the project bundle chat message delimiter")
                : String("Project bundle chat message has an invalid delimiter")};
            break;
        }
        result = writeHistoryMessage(
            staged, expectedSequence, {role, std::move(content.value)});
    }
    if (result.success && !reader.seek(reader.position())) {
        result = {false, "Failed to align the project bundle after chat import"};
    }
    staged.flush();
    staged.close();
    if (result.success) {
        result = validateHistoryFile(stagedHistoryPath, messageCount);
    }
    if (result.success) {
        result = commitStagedSdFile(historyPath, stagedHistoryPath);
    }
    ChatDocument imported = metadata;
    imported.projectId = destinationProjectId;
    imported.messages.clear();
    imported.summary.revision = 1;
    imported.toolPolicy = importedToolPolicy;
    if (result.success) {
        result = rebuildTailFile(destinationProjectId, metadata.summary.id, messageCount);
    }
    if (result.success) {
        result = writeProjectChatMetadata(imported);
    }
    if (result.success) {
        result = upsertProjectChatSummary(destinationProjectId, imported.summary);
    }
    return result.success ? OperationResult{true, ""}
                          : cleanupFailedChatImport(chatDirectory, result.error);
}

}  // namespace cardputer
