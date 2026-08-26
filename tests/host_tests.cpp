#include "../firmware/CardputerAssistant/src/text_utils.h"
#include "../firmware/CardputerAssistant/src/audio_utils.h"
#include "../firmware/CardputerAssistant/src/document_reader.h"
#include "../firmware/CardputerAssistant/src/instruction_policy.h"
#include "../firmware/CardputerAssistant/src/json_string_reader.h"
#include "../firmware/CardputerAssistant/src/offline_tools.h"
#include "../firmware/CardputerAssistant/src/ssh_terminal.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class MemoryJsonReader {
public:
    explicit MemoryJsonReader(const std::string& value) : value_(value), position_(0) {}

    int available() const
    {
        return position_ < value_.size() ? 1 : 0;
    }

    int read()
    {
        return available() ? static_cast<unsigned char>(value_[position_++]) : -1;
    }

    int peek() const
    {
        return available() ? static_cast<unsigned char>(value_[position_]) : -1;
    }

    std::size_t position() const
    {
        return position_;
    }

    bool seek(std::size_t position)
    {
        if (position > value_.size()) return false;
        position_ = position;
        return true;
    }

private:
    const std::string& value_;
    std::size_t position_;
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testUtf8Backspace()
{
    require(cardputer::removeLastUtf8CodePoint("hello") == "hell", "ASCII backspace failed");
    require(cardputer::removeLastUtf8CodePoint("test я") == "test ", "Cyrillic backspace failed");
    require(cardputer::removeLastUtf8CodePoint("") == "", "Empty backspace failed");
    const std::string value = "AяB";
    require(cardputer::previousUtf8Boundary(value, 3) == 1,
            "UTF-8 previous cursor boundary failed");
    require(cardputer::nextUtf8Boundary(value, 1) == 3,
            "UTF-8 next cursor boundary failed");
    require(cardputer::insertUtf8At(value, 3, "!") == "Aя!B",
            "UTF-8 cursor insertion failed");
    require(cardputer::eraseUtf8Before(value, 3) == "AB",
            "UTF-8 cursor erasure failed");
}

void testRussianLayout()
{
    require(cardputer::mapKeyToRussian('q') == "й", "Lowercase Russian layout failed");
    require(cardputer::mapKeyToRussian('Q') == "Й", "Uppercase Russian layout failed");
    require(cardputer::mapKeyToRussian('`') == "ё", "Russian yo mapping failed");
    require(cardputer::mapKeyToRussian('1') == "1", "Numeric key mapping failed");
}

void testWrapping()
{
    const std::vector<std::string> expected = {"ab", "я", "cd"};
    require(cardputer::wrapUtf8Text("abя" "cd", 2) == expected, "UTF-8 wrapping failed");
    require(cardputer::wrapUtf8Text("a\nb", 10).size() == 2, "Newline wrapping failed");
    const std::vector<std::string> wordWrapped = {"one two", "three"};
    require(cardputer::wrapUtf8Text("one two three", 7) == wordWrapped,
            "Word-aware wrapping failed");
    const std::vector<std::string> russianWrapped = {"Ты: Это", "пример", "строки"};
    require(cardputer::wrapUtf8Text("Ты: Это пример строки", 14) == russianWrapped,
            "Cyrillic word-aware wrapping failed");
    const std::string largeUnbrokenText(65536, 'x');
    const auto largeWrapped = cardputer::wrapUtf8Text(largeUnbrokenText, 38);
    require(largeWrapped.size() == 1725, "Large text line count is incorrect");
    std::size_t wrappedBytes = 0;
    for (const auto& line : largeWrapped) {
        require(line.size() <= 38, "Large text line exceeds the requested width");
        wrappedBytes += line.size();
    }
    require(wrappedBytes == largeUnbrokenText.size(), "Large text wrapping lost data");
}

void testSse()
{
    std::string data;
    require(cardputer::extractSseData("data: {\"ok\":true}\r", data), "SSE data line not detected");
    require(data == "{\"ok\":true}", "SSE payload extraction failed");
    require(!cardputer::extractSseData("event: message", data), "Non-data SSE line detected");
    require(cardputer::buildVersionedApiUrl("https://api.example.com", "/v1/models") ==
                "https://api.example.com/v1/models",
            "Unversioned API base URL was joined incorrectly");
    require(cardputer::buildVersionedApiUrl("https://api.example.com/v1", "/v1/chat/completions") ==
                "https://api.example.com/v1/chat/completions",
            "Versioned API base URL duplicated /v1");
}

void testUtf8Validation()
{
    require(cardputer::isValidUtf8("Привет"), "Valid UTF-8 rejected");
    require(!cardputer::isValidUtf8(std::string("\xD0", 1)), "Truncated UTF-8 accepted");
    require(!cardputer::isValidUtf8(std::string("a\0b", 3)),
            "Embedded NUL accepted as application text");
    const std::string largeValid(65536, 'x');
    require(cardputer::isValidUtf8(largeValid), "Large valid UTF-8 rejected");
    std::string largeInvalid = largeValid;
    largeInvalid.push_back(static_cast<char>(0xF0));
    require(!cardputer::isValidUtf8(largeInvalid), "Large truncated UTF-8 accepted");
}

void testWavHeader()
{
    const auto header = cardputer::buildPcmWavHeader(16000, 16000);
    require(std::string(header.begin(), header.begin() + 4) == "RIFF", "WAV RIFF marker failed");
    require(std::string(header.begin() + 8, header.begin() + 12) == "WAVE", "WAV format marker failed");
    require(header[24] == 0x80 && header[25] == 0x3E, "WAV sample rate failed");
    require(header[40] == 0x00 && header[41] == 0x7D, "WAV data size failed");
}

void testChatText()
{
    require(cardputer::makeChatTitle("  Первый\n\tзапрос пользователя  ", 18) ==
                "Первый з...",
            "Cyrillic chat title generation failed");
    require(cardputer::ellipsizeUtf8("Коротко", 20) == "Коротко",
            "Short UTF-8 title changed");
    require(cardputer::isValidChatId("0123456789abcdef"), "Valid chat id rejected");
    require(!cardputer::isValidChatId("0123456789ABCDEF"), "Uppercase chat id accepted");
    require(cardputer::isValidWorkspaceFilename("notes_ru.md"), "Valid workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("automation.py"), "Python workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("chat_export.chat.jsonl"),
            "Portable chat bundle filename rejected");
    require(!cardputer::isValidWorkspaceFilename("../secret.txt"), "Traversal filename accepted");
    require(cardputer::isValidWorkspaceFilename("bin/program.exe"),
            "Arbitrary nested workspace file rejected");
    require(cardputer::isValidWorkspaceFilename(".hidden.txt"),
            "Hidden workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("note.MD"),
            "Uppercase workspace extension rejected");
    const std::vector<std::string> textExtensions = {
        ".txt", ".md", ".json", ".jsonl", ".csv", ".html", ".svg", ".py",
        ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf", ".log", ".xml",
        ".css", ".js", ".mjs", ".cjs", ".ts", ".tsx", ".sh", ".bash", ".zsh",
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", ".ino",
        ".env", ".properties", ".sql",
    };
    for (const std::string& extension : textExtensions) {
        require(cardputer::isWorkspaceTextFile("nested/source" + extension),
                "Safe workspace text extension rejected: " + extension);
        require(cardputer::requestsWorkspaceAccess("inspect nested/source" + extension),
                "Safe workspace text extension did not enable tools: " + extension);
        std::string uppercase = extension;
        for (char& value : uppercase) {
            value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        }
        require(cardputer::isWorkspaceTextFile("nested/source" + uppercase),
                "Uppercase workspace text extension rejected: " + uppercase);
        require(cardputer::requestsWorkspaceAccess("inspect nested/source" + uppercase),
                "Uppercase workspace text extension did not enable tools: " + uppercase);
    }
    const std::vector<std::string> transferOnlyNames = {
        "firmware.bin", "archive.zip", "program.exe", "image.png", "README",
        "notes.txt.exe", "nested/no-extension",
    };
    for (const std::string& name : transferOnlyNames) {
        require(!cardputer::isWorkspaceTextFile(name),
                "Transfer-only workspace file accepted as text: " + name);
        require(!cardputer::requestsWorkspaceAccess("inspect " + name),
                "Transfer-only extension enabled workspace tools: " + name);
    }
    require(!cardputer::isValidWorkspaceFilename("draft.tmp"),
            "Reserved temporary workspace suffix accepted");
    require(!cardputer::isValidWorkspaceFilename("backup.bak"),
            "Reserved backup workspace suffix accepted");
    require(cardputer::isValidStorageRelativePath("Проекты/заметки/идея.md", 512),
            "Valid nested UTF-8 storage path rejected");
    require(cardputer::isValidStorageRelativePath("src/main.cpp", 512),
            "Valid source storage path rejected");
    require(!cardputer::isValidStorageRelativePath("../secret.txt", 512),
            "Parent traversal storage path accepted");
    require(!cardputer::isValidStorageRelativePath("notes//draft.md", 512),
            "Empty storage path segment accepted");
    require(!cardputer::isValidStorageRelativePath("C:/secret.txt", 512),
            "Drive-qualified storage path accepted");
    require(!cardputer::isValidStorageRelativePath("/absolute.txt", 512),
            "Absolute storage path accepted");
    require(!cardputer::isValidStorageRelativePath(std::string("bad\x01.txt", 8), 512),
            "Control character in storage path accepted");
    require(!cardputer::isValidStorageRelativePath("notes/./draft.md", 512),
            "Current-directory storage segment accepted");
    require(!cardputer::isValidStorageRelativePath("notes/../draft.md", 512),
            "Nested parent traversal storage path accepted");
}

void testContextWindowBudget()
{
    const std::vector<cardputer::Message> messages = {
        {"user", "first"},
        {"assistant", "second"},
        {"user", "third"},
    };
    const cardputer::ContextWindowResult all =
        cardputer::fitMessagesToByteBudget(messages, 4096);
    require(all.droppedMessages == 0 && all.retained.size() == 3,
            "Context budget dropped messages that fit");
    const cardputer::ContextWindowResult newest =
        cardputer::fitMessagesToByteBudget(messages, 22);
    require(newest.droppedMessages == 2 && newest.retained.size() == 1 &&
                newest.retained.front().content == "third",
            "Context budget did not retain the newest complete message");
    const cardputer::ContextWindowResult zero =
        cardputer::fitMessagesToByteBudget(messages, 0);
    require(zero.droppedMessages == 3 && zero.retained.empty(),
            "Zero context budget retained messages");
    const std::vector<cardputer::Message> oversized = {{"user", std::string(128, 'x')}};
    const cardputer::ContextWindowResult one =
        cardputer::fitMessagesToByteBudget(oversized, 16);
    require(one.droppedMessages == 0 && one.retained.size() == 1,
            "Context budget discarded the only newest message");
    std::vector<cardputer::Message> owned = {
        {"user", "first"},
        {"assistant", "second"},
        {"user", std::string(16384, 'x')},
    };
    const char* promptStorage = owned.back().content.data();
    const cardputer::ContextWindowResult moved =
        cardputer::fitOwnedMessagesToByteBudget(std::move(owned), 16404);
    require(moved.droppedMessages == 2 && moved.retained.size() == 1 &&
                moved.retained.front().content.data() == promptStorage,
            "Owned context fitting copied the retained large prompt");
    std::vector<cardputer::Message> finalMessages = moved.retained;
    finalMessages.push_back({"assistant", std::string(16384, 'y')});
    const std::vector<cardputer::Message> dropped =
        cardputer::takeMessagesDroppedToByteBudget(std::move(finalMessages), 16409);
    require(dropped.size() == 1 && dropped.front().content.size() == 16384,
            "Owned context fitting did not return the omitted prefix");
}

void testJsonStringReader()
{
    const std::string exactContent(16384, 'x');
    const std::string exactRecord = "{\"sequence\":1,\"content\":\"" +
        exactContent + "\",\"role\":\"user\"}";
    MemoryJsonReader exactReader(exactRecord);
    const cardputer::json_reader::JsonStringValueResult exact =
        cardputer::json_reader::readObjectStringField(
            exactReader, "content", 64, 16384);
    require(exact.success && exact.value == exactContent,
            "Exact JSON string boundary decoding failed");

    const std::string escapedRecord =
        "{\"content\":\"quote: \\\" slash: \\\\ unicode: \\u041f"
        " emoji: \\uD83D\\uDE00\"}";
    MemoryJsonReader escapedReader(escapedRecord);
    const cardputer::json_reader::JsonStringValueResult escaped =
        cardputer::json_reader::readObjectStringField(
            escapedReader, "content", 64, 16384);
    require(escaped.success && escaped.value == "quote: \" slash: \\ unicode: П emoji: 😀",
            "Escaped JSON string decoding failed");

    const std::string duplicateRecord =
        "{\"content\":\"first\",\"content\":\"second\"}";
    MemoryJsonReader duplicateReader(duplicateRecord);
    require(!cardputer::json_reader::readObjectStringField(
                 duplicateReader, "content", 64, 16384).success,
            "Duplicate JSON string field was accepted");

    MemoryJsonReader exactMeasureReader(exactRecord);
    const cardputer::json_reader::JsonStringLengthResult exactMeasure =
        cardputer::json_reader::measureObjectStringField(
            exactMeasureReader, "content", 64, 16384);
    require(exactMeasure.success && exactMeasure.bytes == 16384,
            "Exact JSON string boundary measurement failed");

    const std::string unicodeRecord =
        "{\"ignored\":{\"nested\":true},\"c\\u006Fntent\":\"Привет "
        "\\u041C\\u0438\\u0440 \\uD83D\\uDE00\"}";
    const std::string decodedUnicode = "Привет Мир 😀";
    MemoryJsonReader unicodeReader(unicodeRecord);
    const cardputer::json_reader::JsonStringLengthResult unicode =
        cardputer::json_reader::measureObjectStringField(
            unicodeReader, "content", 64, 16384);
    require(unicode.success && unicode.bytes == decodedUnicode.size(),
            "Raw and escaped Unicode JSON measurement failed");

    const std::string maximumContent(131072, 'm');
    const std::string maximumRecord = "{\"content\":\"" + maximumContent + "\"}";
    MemoryJsonReader maximumReader(maximumRecord);
    const cardputer::json_reader::JsonStringLengthResult maximum =
        cardputer::json_reader::measureObjectStringField(
            maximumReader, "content", 64, 131072);
    require(maximum.success && maximum.bytes == 131072,
            "Maximum JSON string measurement failed");

    const std::string emptyRecord = "{\"content\":\"\"}";
    MemoryJsonReader emptyReader(emptyRecord);
    const cardputer::json_reader::JsonStringLengthResult empty =
        cardputer::json_reader::measureObjectStringField(
            emptyReader, "content", 64, 16384);
    require(empty.success && empty.bytes == 0,
            "Empty JSON string measurement did not remain caller-controlled");

    const std::string oversizedRecord =
        "{\"content\":\"" + std::string(131073, 'o') + "\"}";
    MemoryJsonReader oversizedReader(oversizedRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 oversizedReader, "content", 64, 131072).success,
            "JSON string above the measurement limit was accepted");

    const std::string malformedEscapeRecord = "{\"content\":\"bad\\q\"}";
    MemoryJsonReader malformedEscapeReader(malformedEscapeRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 malformedEscapeReader, "content", 64, 16384).success,
            "Malformed JSON escape was accepted during measurement");

    const std::string malformedSurrogateRecord =
        "{\"content\":\"bad \\uD83Dvalue\"}";
    MemoryJsonReader malformedSurrogateReader(malformedSurrogateRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 malformedSurrogateReader, "content", 64, 16384).success,
            "Malformed JSON surrogate pair was accepted during measurement");

    std::string invalidUtf8Record = "{\"content\":\"bad ";
    invalidUtf8Record.push_back(static_cast<char>(0xF0));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record += "\"}";
    MemoryJsonReader invalidUtf8Reader(invalidUtf8Record);
    require(!cardputer::json_reader::measureObjectStringField(
                 invalidUtf8Reader, "content", 64, 16384).success,
            "Invalid literal UTF-8 was accepted during JSON measurement");

    MemoryJsonReader measuredDuplicateReader(duplicateRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 measuredDuplicateReader, "content", 64, 16384).success,
            "Duplicate JSON string field was accepted during measurement");

    const std::string missingRecord = "{\"role\":\"user\"}";
    MemoryJsonReader missingReader(missingRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 missingReader, "content", 64, 16384).success,
            "Missing JSON string field was accepted during measurement");

    const std::string trailingRecord = "{\"content\":\"ok\"}garbage";
    MemoryJsonReader trailingReader(trailingRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 trailingReader, "content", 64, 16384).success,
            "Trailing JSON data was accepted during measurement");
}

void testLargePromptTitle()
{
    const std::string prompt = std::string(16384, 'x');
    const std::string title = cardputer::makeChatTitle(prompt, 36);
    require(title == std::string(33, 'x') + "...",
            "Large prompt title was not bounded to its display-cell budget");
    std::string unicodePrompt;
    unicodePrompt.reserve(16384);
    for (std::size_t index = 0; index < 5461; ++index) {
        unicodePrompt += "\xE7\x94\xA8";
    }
    unicodePrompt += 'x';
    const std::string unicodeTitle = cardputer::makeChatTitle(unicodePrompt, 36);
    require(unicodePrompt.size() == 16384 && unicodeTitle.size() <= 51 &&
                unicodeTitle.size() >= 3 &&
                unicodeTitle.compare(unicodeTitle.size() - 3, 3, "...") == 0,
            "Large Unicode prompt title was not bounded without full-prompt growth");
}

void testInstructionPrecedence()
{
    const std::string scoped = cardputer::buildScopedInstructions(
        "project", "chat", "request", "summary");
    const std::string resolved = cardputer::buildUserInstructionScopes("global", scoped);
    const std::size_t global = resolved.find("Global instructions");
    const std::size_t project = resolved.find("Project instructions");
    const std::size_t chat = resolved.find("Chat-specific instructions");
    const std::size_t request = resolved.find("Instructions for this request");
    const std::size_t summary = resolved.find("Conversation summary");
    require(global != std::string::npos && global < project && project < chat &&
                chat < summary && summary < request,
            "Scoped instruction precedence is not global -> project -> chat -> request");
    require(cardputer::buildScopedInstructions("", "chat", "", "").find(
                "Project instructions supplied by the user") == std::string::npos,
            "Empty instruction scope produced a synthetic section");
    require(cardputer::kMaximumRequestInstructionsBytes == 2048,
            "One-request instruction limit changed unexpectedly");
}

void testRequestOutputBudget()
{
    require(cardputer::resolveRequestOutputTokens(2048, 0) == 2048,
            "Missing request output override did not preserve the project budget");
    require(cardputer::resolveRequestOutputTokens(2048, 8192) == 8192,
            "Request output override did not take precedence over the project budget");
}

void testProjectRequestPolicy()
{
    cardputer::Settings settings = {};
    settings.model = "global-model";
    cardputer::ProjectDocument project = {};
    project.contextByteBudget = 65536;
    project.maximumOutputTokens = 2048;
    project.automaticCompaction = true;
    const cardputer::ResolvedProjectRequestPolicy inherited =
        cardputer::resolveProjectRequestPolicy(settings, project, 0);
    require(inherited.model == "global-model" &&
                inherited.contextByteBudget == 65536 &&
                inherited.maximumOutputTokens == 2048 &&
                inherited.automaticCompaction,
            "Project request policy did not inherit global/project defaults");
    require(!cardputer::shouldAutomaticallyCompactRequest(inherited, 0) &&
                cardputer::shouldAutomaticallyCompactRequest(inherited, 1),
            "Automatic compaction gate ignored the dropped-message boundary");

    project.model = "project-model";
    project.automaticCompaction = false;
    const cardputer::ResolvedProjectRequestPolicy overridden =
        cardputer::resolveProjectRequestPolicy(settings, project, 8192);
    require(overridden.model == "project-model" &&
                overridden.maximumOutputTokens == 8192 &&
                !overridden.automaticCompaction &&
                !cardputer::shouldAutomaticallyCompactRequest(overridden, 4),
            "Project/request overrides did not resolve deterministically");
}

void testContextUsage()
{
    cardputer::ChatDocument chat = {};
    chat.summary.messageCount = 10;
    chat.summarizedMessageCount = 2;
    chat.messages = {
        {"user", "a"},
        {"assistant", "bb"},
        {"user", "ccc"},
        {"assistant", "dddd"},
    };
    const cardputer::ContextUsage bounded = cardputer::resolveContextUsage(chat, 29);
    require(bounded.retainedBytes == 29 && bounded.retainedMessages == 1 &&
                bounded.droppedMessages == 7 && bounded.summarizedMessages == 2 &&
                bounded.totalMessages == 10,
            "Context usage did not include the bounded-tail and role overhead");
    const cardputer::ContextUsage oversizedNewest =
        cardputer::resolveContextUsage(chat, 1);
    require(oversizedNewest.retainedBytes == 29 &&
                oversizedNewest.retainedMessages == 1 &&
                oversizedNewest.droppedMessages == 7,
            "Context usage did not preserve the newest oversized message");
    const cardputer::ContextUsage disabled = cardputer::resolveContextUsage(chat, 0);
    require(disabled.retainedBytes == 0 && disabled.retainedMessages == 0 &&
                disabled.droppedMessages == 8,
            "Zero context budget retained unsummarized messages");
}

void testRetryRequestPreparation()
{
    const std::vector<cardputer::Message> failedHistory = {
        {"user", "first"},
        {"assistant", "answer"},
        {"user", "retry exactly once"},
    };
    const cardputer::RetryRequestResult retry = cardputer::prepareRetryRequest(
        failedHistory, 4096);
    require(retry.success && retry.prompt == "retry exactly once",
            "Retry preparation did not retain the failed user request");
    require(retry.messages.size() == failedHistory.size(),
            "Retry preparation duplicated or removed a stored message");
    require(failedHistory.size() == 3 && failedHistory.back().content == "retry exactly once",
            "Retry preparation modified its input history");
    require(!cardputer::prepareRetryRequest(
                {{"user", "done"}, {"assistant", "answer"}}, 4096).success,
            "Retry preparation accepted a completed assistant turn");
}

void testSummarizedChatTail()
{
    cardputer::ChatDocument chat = {};
    chat.summary.messageCount = 12;
    chat.summarizedMessageCount = 8;
    chat.messages = {
        {"user", "6"},
        {"assistant", "7"},
        {"user", "8"},
        {"assistant", "9"},
        {"user", "10"},
        {"assistant", "11"},
    };
    const std::vector<cardputer::Message> active =
        cardputer::unsummarizedChatTail(chat);
    require(active.size() == 4 && active.front().content == "8" &&
                active.back().content == "11",
            "Summarized messages remained in the active context tail");
    require(chat.messages.size() == 6,
            "Active-tail calculation modified raw chat messages");
}

void testContextSummaryPrompt()
{
    const std::vector<cardputer::Message> messages = {
        {"user", "first"},
        {"assistant", "second"},
    };
    const cardputer::ContextSummaryPromptResult result =
        cardputer::buildContextSummaryPrompt("earlier", messages, 1024);
    require(result.success, "Context summary prompt should fit");
    require(result.includedMessages == 2,
            "Context summary prompt should include both messages");
    require(result.prompt.find("Previous summary:\nearlier") != std::string::npos,
            "Context summary prompt should preserve the previous summary");
    require(result.prompt.find("You: first\nAI: second\n") != std::string::npos,
            "Context summary prompt should preserve roles and content");
    const cardputer::ContextSummaryPromptResult tooSmall =
        cardputer::buildContextSummaryPrompt("earlier", messages, 32);
    require(!tooSmall.success,
            "Context summary prompt should reject an insufficient allocation budget");
}

void testWorkspaceRouting()
{
    require(!cardputer::requestsWorkspaceAccess("Когда появится Cardputer Zero?"),
            "Ordinary Russian question enabled workspace tools");
    require(!cardputer::requestsWorkspaceAccess("Read the question and answer briefly"),
            "Ordinary English instruction enabled workspace tools");
    require(cardputer::requestsWorkspaceAccess("Сохрани ответ в файл release.md"),
            "Explicit Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess(
                "Можешь набросапть простой тестовый скрипт на Python, и сохранить его?"),
            "Natural Russian Python save request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("ПОКАЖИ, КАКИЕ ФАЙЛЫ ЕСТЬ НА SD"),
            "Uppercase Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("/file create notes.txt"),
            "Explicit file command did not enable workspace tools");
    require(cardputer::requestsWorkspaceWrite(
                "Можешь набросапть простой тестовый скрипт на Python, и сохранить его?"),
            "Natural Russian Python save request did not require a file write");
    require(cardputer::requestsWorkspaceWrite("Create a downloadable file notes.md"),
            "English create request did not require a file write");
    require(!cardputer::requestsWorkspaceWrite("ПОКАЖИ, КАКИЕ ФАЙЛЫ ЕСТЬ НА SD"),
            "Read-only file request incorrectly required a file write");
    std::string inertBoundaryPrompt;
    inertBoundaryPrompt.reserve(16384);
    for (std::size_t index = 0; index < 5461; ++index) {
        inertBoundaryPrompt += "用";
    }
    inertBoundaryPrompt += 'x';
    require(inertBoundaryPrompt.size() == 16384,
            "Workspace routing fixture is not exactly 16384 bytes");
    require(!cardputer::requestsWorkspaceAccess(inertBoundaryPrompt) &&
                !cardputer::requestsWorkspaceWrite(inertBoundaryPrompt) &&
                !cardputer::requestsWebSearch(inertBoundaryPrompt),
            "Inert exact-boundary prompt enabled a tool policy");
    const std::string trailingCommand = " СОХРАНИ В ФАЙЛ";
    std::string commandBoundaryPrompt(16384 - trailingCommand.size(), 'x');
    commandBoundaryPrompt += trailingCommand;
    require(cardputer::requestsWorkspaceAccess(commandBoundaryPrompt) &&
                cardputer::requestsWorkspaceWrite(commandBoundaryPrompt),
            "Exact-boundary trailing Russian file command was not scanned");
}

void testWebSearchRouting()
{
    require(cardputer::requestsWebSearch("Когда появится Cardputer Zero?"),
            "Current Russian question did not enable web search");
    require(cardputer::requestsWebSearch("Find the latest Cardputer firmware news"),
            "Current English question did not enable web search");
    require(cardputer::requestsWebSearch("/search Cardputer ADV"),
            "Explicit search command did not enable web search");
    require(!cardputer::requestsWebSearch("Напиши краткое эссе о кошках"),
            "Ordinary Russian request enabled web search");
    require(!cardputer::requestsWebSearch("What is two plus two?"),
            "Stable English question enabled web search");
    require(cardputer::isWebSearchToolName("web_search"),
            "Canonical web search tool name was rejected");
    require(cardputer::isWebSearchToolName("WebSearch"),
            "Proxy web search tool alias was rejected");
    require(cardputer::isWebSearchToolName("web-search"),
            "Hyphenated web search tool alias was rejected");
    require(!cardputer::isWebSearchToolName("write_file"),
            "File tool name was classified as web search");
    require(cardputer::isWebFetchToolName("WebFetch"),
            "Proxy web fetch tool alias was rejected");
    require(cardputer::isWebFetchToolName("web_fetch"),
            "Canonical web fetch tool name was rejected");
    require(!cardputer::isWebFetchToolName("read_file"),
            "File tool name was classified as web fetch");
}

void testSshTerminalFiltering()
{
    cardputer::SshTerminalText state = {"prompt", "", false};
    const std::string colored = "\x1B[31m red\x1B[0m\nnext\rreplace";
    state = cardputer::appendSshTerminalBytes(
        state, reinterpret_cast<const std::uint8_t*>(colored.data()),
        colored.size(), 1024);
    require(state.text == "prompt red\nreplace", "SSH ANSI or carriage-return filtering failed");
    const std::string clear = "\x1B[2Jclean";
    state = cardputer::appendSshTerminalBytes(
        state, reinterpret_cast<const std::uint8_t*>(clear.data()),
        clear.size(), 1024);
    require(state.text == "clean", "SSH clear-screen filtering failed");
    const auto lines = cardputer::sshTerminalVisibleLines(state, 10, 3);
    require(lines.size() == 3 && lines.back() == "clean",
            "SSH terminal viewport failed");
    const std::string history = "one\ntwo\nthree\nfour\nfive";
    state = {history, "", false};
    const auto scrolled = cardputer::sshTerminalLinesFromBottom(state, 10, 2, 2);
    require(scrolled.size() == 2 && scrolled[0] == "two" && scrolled[1] == "three",
            "SSH terminal scrollback offset failed");
    const auto bounded = cardputer::sshTerminalLinesFromBottom(state, 10, 2, 99);
    require(bounded.size() == 2 && bounded[0] == "one" && bounded[1] == "two",
            "SSH terminal scrollback bound failed");
}

void testDocumentReader()
{
    using cardputer::DocumentReaderMode;
    require(cardputer::detectDocumentReaderMode("notes.MD") == DocumentReaderMode::Markdown,
            "Markdown reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("table.csv") == DocumentReaderMode::Csv,
            "CSV reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("data.json") == DocumentReaderMode::Json,
            "JSON reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("page.HTML") == DocumentReaderMode::HtmlSource,
            "HTML source reader mode detection failed");
    require(cardputer::formatDocumentChunk(DocumentReaderMode::Markdown,
                                            "# Title\n- item") ==
                "Title\n• item",
            "Markdown reader formatting failed");
    require(cardputer::formatDocumentChunk(DocumentReaderMode::Csv,
                                            "name,\"one,two\"") ==
                "name | one,two",
            "Quoted CSV reader formatting failed");
    const std::string json = cardputer::formatDocumentChunk(
        DocumentReaderMode::Json, "{\"ok\":true,\"n\":2}");
    require(json.find("\n") != std::string::npos &&
                json.find("\"ok\": true") != std::string::npos,
            "JSON reader formatting failed");
    require(cardputer::documentSpeechText(DocumentReaderMode::HtmlSource,
                                           "<p>Hello &amp; bye</p>") ==
                "Hello & bye ",
            "HTML speech extraction failed");
}

void testOfflineCalculator()
{
    const cardputer::CalculationResult precedence = cardputer::calculateExpression("2+3*4");
    require(precedence.success && precedence.value == 14.0,
            "Calculator precedence failed");
    const cardputer::CalculationResult parentheses = cardputer::calculateExpression("(2+3)*4");
    require(parentheses.success && parentheses.value == 20.0,
            "Calculator parentheses failed");
    const cardputer::CalculationResult unary = cardputer::calculateExpression("-2.5 + 1");
    require(unary.success && unary.value == -1.5, "Calculator unary operator failed");
    require(!cardputer::calculateExpression("1/0").success,
            "Calculator accepted division by zero");
    require(!cardputer::calculateExpression("2+").success,
            "Calculator accepted an incomplete expression");
    require(cardputer::formatCalculationResult(1.25) == "1.25",
            "Calculator result formatting failed");
}

}  // namespace

int main()
{
    try {
        testUtf8Backspace();
        testRussianLayout();
        testWrapping();
        testSse();
        testUtf8Validation();
        testJsonStringReader();
        testWavHeader();
        testChatText();
        testContextWindowBudget();
        testLargePromptTitle();
        testInstructionPrecedence();
        testRequestOutputBudget();
        testProjectRequestPolicy();
        testContextUsage();
        testRetryRequestPreparation();
        testSummarizedChatTail();
        testContextSummaryPrompt();
        testWorkspaceRouting();
        testWebSearchRouting();
        testSshTerminalFiltering();
        testDocumentReader();
        testOfflineCalculator();
        std::cout << "host_tests: PASS\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "host_tests: FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
