#include "../firmware/CardputerAssistant/src/text_utils.h"
#include "../firmware/CardputerAssistant/src/audio_utils.h"
#include "../firmware/CardputerAssistant/src/document_reader.h"
#include "../firmware/CardputerAssistant/src/offline_tools.h"
#include "../firmware/CardputerAssistant/src/ssh_terminal.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
    require(cardputer::isValidWorkspaceFilename("chat_export.chat.jsonl"),
            "Portable chat bundle filename rejected");
    require(!cardputer::isValidWorkspaceFilename("../secret.txt"), "Traversal filename accepted");
    require(!cardputer::isValidWorkspaceFilename("program.exe"), "Executable extension accepted");
    require(!cardputer::isValidWorkspaceFilename(".hidden.txt"), "Hidden filename accepted");
    require(!cardputer::isValidWorkspaceFilename("note.MD"), "Uppercase extension accepted");
}

void testWorkspaceRouting()
{
    require(!cardputer::requestsWorkspaceAccess("Когда появится Cardputer Zero?"),
            "Ordinary Russian question enabled workspace tools");
    require(!cardputer::requestsWorkspaceAccess("Read the question and answer briefly"),
            "Ordinary English instruction enabled workspace tools");
    require(cardputer::requestsWorkspaceAccess("Сохрани ответ в файл release.md"),
            "Explicit Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("ПОКАЖИ, КАКИЕ ФАЙЛЫ ЕСТЬ НА SD"),
            "Uppercase Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("/file create notes.txt"),
            "Explicit file command did not enable workspace tools");
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
        testWavHeader();
        testChatText();
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
