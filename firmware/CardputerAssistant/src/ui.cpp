#include "ui.h"

#include "text_utils.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace cardputer {
namespace {

constexpr std::size_t kTranscriptCells = 38;
constexpr std::size_t kIdleTranscriptLines = 7;
constexpr std::size_t kCompactTranscriptLines = 6;
constexpr std::size_t kDetailedTranscriptLines = 5;
std::unique_ptr<LGFX_Sprite> canvas;

const std::uint8_t kWifiIcon[] PROGMEM = {
    0x00, 0x3C, 0x42, 0x99, 0x24, 0x18, 0x00, 0x18,
};
const std::uint8_t kTrashIcon[] PROGMEM = {
    0x18, 0x7E, 0x42, 0x5A, 0x5A, 0x42, 0x3C, 0x00,
};
const std::uint8_t kChatsIcon[] PROGMEM = {
    0x00, 0x78, 0x84, 0xFE, 0x82, 0x82, 0x7C, 0x00,
};
const std::uint8_t kModelIcon[] PROGMEM = {
    0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C,
};
const std::uint8_t kLanguageIcon[] PROGMEM = {
    0x3C, 0x5A, 0xA5, 0xFF, 0xA5, 0x5A, 0x3C, 0x00,
};
const std::uint8_t kSettingsIcon[] PROGMEM = {
    0x18, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x18,
};
const std::uint8_t kMicIcon[] PROGMEM = {
    0x18, 0x3C, 0x3C, 0x3C, 0x3C, 0x5A, 0x3C, 0x18,
};
const std::uint8_t kUpIcon[] PROGMEM = {
    0x18, 0x3C, 0x7E, 0xDB, 0x18, 0x18, 0x18, 0x00,
};
const std::uint8_t kDownIcon[] PROGMEM = {
    0x18, 0x18, 0x18, 0xDB, 0x7E, 0x3C, 0x18, 0x00,
};

struct TranscriptLine {
    std::string text;
    std::uint16_t color;
};

std::vector<TranscriptLine> transcriptLines(const std::vector<Message>& history,
                                            const std::string& activeResponse)
{
    std::vector<TranscriptLine> lines;
    for (const auto& message : history) {
        const std::string prefix = message.role == "user" ? "You: " : "AI: ";
        const auto wrapped = wrapUtf8Text(prefix + message.content, kTranscriptCells);
        const std::uint16_t color = message.role == "user" ? TFT_CYAN : TFT_LIGHTGREY;
        for (const auto& line : wrapped) {
            lines.push_back({line, color});
        }
    }
    if (!activeResponse.empty()) {
        const auto wrapped = wrapUtf8Text("AI: " + activeResponse, kTranscriptCells);
        for (const auto& line : wrapped) {
            lines.push_back({line, TFT_GREENYELLOW});
        }
    }
    return lines;
}

String clippedLine(const String& value, std::size_t maximumLength)
{
    return String(ellipsizeUtf8(value.c_str(), maximumLength).c_str());
}

bool isErrorStatus(const String& status)
{
    return status.indexOf("Failed") >= 0 || status.indexOf("failed") >= 0 ||
           status.indexOf("HTTP") >= 0 || status.indexOf("timed out") >= 0 ||
           status.indexOf("not in") >= 0 || status.indexOf("invalid") >= 0 ||
           status.indexOf("Error") >= 0 || status.indexOf("error") >= 0;
}

std::size_t visibleTranscriptLineCount(const String& status)
{
    if (status.isEmpty() || status == "Ready") {
        return kIdleTranscriptLines;
    }
    const String visibleStatus = status.isEmpty() ? String("Ready") : status;
    const auto statusLines = wrapUtf8Text(visibleStatus.c_str(), kTranscriptCells);
    return isErrorStatus(status) && statusLines.size() > 1
        ? kDetailedTranscriptLines
        : kCompactTranscriptLines;
}

void drawToolbarItem(int x, const std::uint8_t* icon, const char* label)
{
    canvas->drawBitmap(x + 2, 125, icon, 8, 8, TFT_WHITE);
    canvas->setCursor(x + 11, 124);
    canvas->print(label);
}

}  // namespace

OperationResult beginUi()
{
    canvas = std::make_unique<LGFX_Sprite>(&M5Cardputer.Display);
    if (canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height()) == nullptr) {
        return {false, "Failed to allocate 240x135 display canvas"};
    }
    canvas->setFont(&fonts::efontCN_12);
    canvas->setTextSize(1);
    canvas->setTextWrap(false);
    canvas->setBaseColor(TFT_BLACK);
    return {true, ""};
}

void showFatalError(const String& error)
{
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setFont(&fonts::efontCN_12);
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.setCursor(4, 4);
    M5Cardputer.Display.fillTriangle(224, 3, 237, 25, 211, 25, TFT_RED);
    M5Cardputer.Display.setTextColor(TFT_BLACK, TFT_RED);
    M5Cardputer.Display.drawCenterString("!", 224, 10);
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.println("FATAL ERROR");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.println(error);
}

void showProvisioning(const String& accessPointName, const String& accessPointPassword)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_14);
    canvas->setTextColor(TFT_CYAN, TFT_BLACK);
    canvas->setCursor(5, 5);
    canvas->print("LOCAL SETUP");
    canvas->drawBitmap(222, 5, kWifiIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(5, 23);
    canvas->print("Wi-Fi:");
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas->setCursor(5, 41);
    canvas->print(accessPointName);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(5, 59);
    canvas->print("Password:");
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas->setCursor(5, 77);
    canvas->print(accessPointPassword);
    canvas->setTextColor(TFT_CYAN, TFT_BLACK);
    canvas->setCursor(5, 97);
    canvas->print("Open in Safari:");
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(5, 115);
    canvas->print("192.168.4.1");
    canvas->pushSprite(0, 0);
}

void showFilesPortal(const String& accessPointName, const String& accessPointPassword)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_14);
    canvas->setTextColor(TFT_CYAN, TFT_BLACK);
    canvas->setCursor(5, 5);
    canvas->print("FILES DOWNLOAD");
    canvas->drawBitmap(222, 5, kChatsIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(5, 24);
    canvas->print("Wi-Fi:");
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas->setCursor(5, 42);
    canvas->print(clippedLine(accessPointName, 28));
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(5, 60);
    canvas->print("Password:");
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas->setCursor(5, 78);
    canvas->print(accessPointPassword);
    canvas->setTextColor(TFT_CYAN, TFT_BLACK);
    canvas->setCursor(5, 98);
    canvas->print("Open 192.168.4.1");
    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setCursor(5, 120);
    canvas->print("Use Restart button to return");
    canvas->pushSprite(0, 0);
}

void showChat(const std::vector<Message>& history,
              const std::string& activeResponse,
              const std::string& input,
              KeyboardLayout layout,
              const String& chatTitle,
              const String& status,
              std::size_t scrollOffset,
              bool wifiConnected)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_12);
    canvas->fillRect(0, 0, 240, 14, TFT_NAVY);
    canvas->drawBitmap(3, 3, kChatsIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->setCursor(14, 2);
    canvas->print(clippedLine(chatTitle, 23));
    canvas->drawBitmap(188, 3, kWifiIcon, 8, 8, wifiConnected ? TFT_GREENYELLOW : TFT_DARKGREY);
    canvas->fillRoundRect(203, 1, 34, 12, 3, layout == KeyboardLayout::English ? TFT_BLUE : TFT_MAROON);
    canvas->setTextColor(TFT_WHITE, layout == KeyboardLayout::English ? TFT_BLUE : TFT_MAROON);
    canvas->setCursor(206, 2);
    canvas->print(layout == KeyboardLayout::English ? "EN" : "RU");

    const auto lines = transcriptLines(history, activeResponse);
    const std::size_t visibleTranscriptLines = visibleTranscriptLineCount(status);
    const std::size_t availableStart = lines.size() > visibleTranscriptLines
        ? lines.size() - visibleTranscriptLines
        : 0;
    const std::size_t start = scrollOffset > availableStart ? 0 : availableStart - scrollOffset;
    const std::size_t end = std::min(lines.size(), start + visibleTranscriptLines);
    int y = 17;
    for (std::size_t index = start; index < end; ++index) {
        canvas->setTextColor(lines[index].color, TFT_BLACK);
        canvas->setCursor(3, y);
        canvas->print(lines[index].text.c_str());
        y += 12;
    }
    if (availableStart > 0) {
        canvas->drawBitmap(230, 18, kUpIcon, 8, 8, scrollOffset < availableStart ? TFT_CYAN : TFT_DARKGREY);
        const int downIconY = 18 + static_cast<int>((visibleTranscriptLines - 1) * 12);
        canvas->drawBitmap(230, downIconY, kDownIcon, 8, 8, scrollOffset > 0 ? TFT_CYAN : TFT_DARKGREY);
    }

    if (visibleTranscriptLines != kIdleTranscriptLines) {
        const std::uint16_t statusColor = isErrorStatus(status) ? TFT_MAROON : TFT_DARKGREY;
        const auto statusLines = wrapUtf8Text(status.c_str(), kTranscriptCells);
        const bool detailedStatus = visibleTranscriptLines == kDetailedTranscriptLines;
        const int statusY = detailedStatus ? 78 : 90;
        const int statusHeight = detailedStatus ? 26 : 14;
        canvas->fillRect(0, statusY, 240, statusHeight, statusColor);
        canvas->setTextColor(TFT_WHITE, statusColor);
        const std::size_t maximumStatusLines = detailedStatus ? 2U : 1U;
        const std::size_t visibleStatusLines = std::min(statusLines.size(), maximumStatusLines);
        for (std::size_t index = 0; index < visibleStatusLines; ++index) {
            canvas->setCursor(3, statusY + static_cast<int>(index * 12));
            canvas->print(statusLines[index].c_str());
        }
    }

    canvas->drawFastHLine(0, 104, 240, TFT_DARKGREY);
    canvas->setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    canvas->setCursor(3, 106);
    const auto inputLines = wrapUtf8Text("> " + input + "_", kTranscriptCells);
    canvas->print(inputLines.back().c_str());

    canvas->fillRect(0, 121, 240, 14, TFT_DARKGREY);
    canvas->drawFastVLine(47, 121, 14, TFT_BLACK);
    canvas->drawFastVLine(95, 121, 14, TFT_BLACK);
    canvas->drawFastVLine(143, 121, 14, TFT_BLACK);
    canvas->drawFastVLine(191, 121, 14, TFT_BLACK);
    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_WHITE, TFT_DARKGREY);
    drawToolbarItem(0, kMicIcon, "G0 MIC");
    drawToolbarItem(48, kChatsIcon, "F1 CHAT");
    drawToolbarItem(96, kModelIcon, "F2 AI");
    drawToolbarItem(144, kLanguageIcon,
                    layout == KeyboardLayout::English ? "F3 RU" : "F3 EN");
    drawToolbarItem(192, kSettingsIcon, "F4 MENU");
    canvas->pushSprite(0, 0);
}

std::size_t maximumChatScrollOffset(const std::vector<Message>& history,
                                    const std::string& activeResponse,
                                    const String& status)
{
    const auto lines = transcriptLines(history, activeResponse);
    const std::size_t visibleTranscriptLines = visibleTranscriptLineCount(status);
    return lines.size() > visibleTranscriptLines ? lines.size() - visibleTranscriptLines : 0;
}

void showSelectionList(const String& title,
                       const std::vector<String>& items,
                       std::size_t selectedIndex,
                       const String& footer)
{
    constexpr std::size_t maximumVisibleItems = 5;
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_14);
    canvas->fillRect(0, 0, 240, 17, TFT_NAVY);
    canvas->drawBitmap(4, 4, kSettingsIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->setCursor(16, 1);
    canvas->print(clippedLine(title, 26));

    if (items.empty()) {
        canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        canvas->setCursor(8, 47);
        canvas->print("No items available");
    } else {
        const std::size_t boundedSelection = std::min(selectedIndex, items.size() - 1);
        std::size_t start = boundedSelection >= maximumVisibleItems
            ? boundedSelection - maximumVisibleItems + 1
            : 0;
        if (items.size() - start < maximumVisibleItems && items.size() > maximumVisibleItems) {
            start = items.size() - maximumVisibleItems;
        }
        canvas->setFont(&fonts::efontCN_12);
        for (std::size_t row = 0; row < maximumVisibleItems && start + row < items.size(); ++row) {
            const std::size_t index = start + row;
            const int y = 20 + static_cast<int>(row * 19);
            const bool selected = index == boundedSelection;
            const std::uint16_t background = selected ? TFT_DARKCYAN : TFT_BLACK;
            canvas->fillRoundRect(3, y, 234, 17, 3, background);
            canvas->setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, background);
            canvas->setCursor(7, y + 1);
            canvas->print(selected ? "> " : "  ");
            canvas->print(clippedLine(items[index], 31));
        }
    }

    canvas->fillRect(0, 117, 240, 18, TFT_DARKGREY);
    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_WHITE, TFT_DARKGREY);
    canvas->setCursor(4, 120);
    canvas->print(clippedLine(footer, 44));
    canvas->pushSprite(0, 0);
}

void showPasswordEntry(const String& ssid, std::size_t passwordLength, const String& status)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_14);
    canvas->fillRect(0, 0, 240, 17, TFT_NAVY);
    canvas->drawBitmap(4, 4, kWifiIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_NAVY);
    canvas->setCursor(16, 1);
    canvas->print("WI-FI PASSWORD");
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setCursor(5, 25);
    canvas->print(clippedLine(ssid, 29));
    canvas->drawRoundRect(4, 48, 232, 25, 4, TFT_DARKGREY);
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas->setCursor(9, 52);
    String mask;
    const std::size_t visibleCharacters = std::min<std::size_t>(passwordLength, 25);
    for (std::size_t index = 0; index < visibleCharacters; ++index) {
        mask += '*';
    }
    if (passwordLength > visibleCharacters) {
        mask += "...";
    }
    canvas->print(mask + "_");
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setCursor(5, 80);
    canvas->print("Length: " + String(passwordLength));
    canvas->setTextColor(isErrorStatus(status) ? TFT_RED : TFT_CYAN, TFT_BLACK);
    canvas->setCursor(5, 98);
    canvas->print(clippedLine(status, 34));
    canvas->fillRect(0, 117, 240, 18, TFT_DARKGREY);
    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_WHITE, TFT_DARKGREY);
    canvas->setCursor(4, 120);
    canvas->print("ENTER connect   FN+` cancel");
    canvas->pushSprite(0, 0);
}

void showBusyScreen(const String& title, const String& message)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->setFont(&fonts::efontCN_14);
    canvas->setTextColor(TFT_CYAN, TFT_BLACK);
    canvas->setCursor(7, 26);
    canvas->print(clippedLine(title, 27));
    canvas->drawBitmap(220, 27, kWifiIcon, 8, 8, TFT_CYAN);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setCursor(7, 57);
    canvas->print(clippedLine(message, 29));
    canvas->pushSprite(0, 0);
}

void showConfirmation(const String& title, const String& message, const String& footer)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->fillRect(0, 0, 240, 18, TFT_MAROON);
    canvas->setFont(&fonts::efontCN_14);
    canvas->drawBitmap(5, 5, kTrashIcon, 8, 8, TFT_WHITE);
    canvas->setTextColor(TFT_WHITE, TFT_MAROON);
    canvas->setCursor(18, 1);
    canvas->print(clippedLine(title, 25));
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    const auto lines = wrapUtf8Text(message.c_str(), 31);
    for (std::size_t index = 0; index < std::min<std::size_t>(lines.size(), 4); ++index) {
        canvas->setCursor(8, 29 + static_cast<int>(index * 17));
        canvas->print(lines[index].c_str());
    }
    canvas->fillRect(0, 117, 240, 18, TFT_DARKGREY);
    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_WHITE, TFT_DARKGREY);
    canvas->setCursor(4, 120);
    canvas->print(clippedLine(footer, 44));
    canvas->pushSprite(0, 0);
}

void showVoiceRecording(std::uint32_t elapsedMs,
                        std::uint32_t maximumMs,
                        std::uint16_t level)
{
    canvas->fillScreen(TFT_BLACK);
    canvas->fillRect(0, 0, 240, 18, TFT_MAROON);
    canvas->setFont(&fonts::efontCN_14);
    canvas->setTextColor(TFT_WHITE, TFT_MAROON);
    canvas->setCursor(6, 1);
    canvas->print("VOICE RECORDING");

    canvas->fillRoundRect(105, 29, 30, 43, 12, TFT_RED);
    canvas->fillRoundRect(111, 34, 18, 31, 8, TFT_WHITE);
    canvas->drawRoundRect(99, 45, 42, 36, 16, TFT_RED);
    canvas->fillRect(117, 80, 6, 10, TFT_RED);
    canvas->fillRoundRect(108, 88, 24, 5, 2, TFT_RED);

    const int meterWidth = static_cast<int>(std::min<std::uint16_t>(level, 1000) * 210U / 1000U);
    canvas->drawRoundRect(14, 99, 212, 9, 3, TFT_DARKGREY);
    canvas->fillRoundRect(15, 100, meterWidth, 7, 2, TFT_GREENYELLOW);
    const std::uint32_t boundedElapsed = std::min(elapsedMs, maximumMs);
    const int progressWidth = maximumMs == 0
        ? 0
        : static_cast<int>(boundedElapsed * 232U / maximumMs);
    canvas->fillRect(4, 113, progressWidth, 4, TFT_RED);

    canvas->setFont(&fonts::efontCN_10);
    canvas->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas->setCursor(4, 122);
    canvas->print("Release G0: transcribe   max ");
    canvas->print(maximumMs / 1000U);
    canvas->print("s");
    canvas->pushSprite(0, 0);
}

bool fontSupportsCyrillic()
{
    lgfx::FontMetrics metric;
    return fonts::efontCN_12.updateFontMetric(&metric, 0x041F) &&
           fonts::efontCN_12.updateFontMetric(&metric, 0x044F);
}

}  // namespace cardputer
