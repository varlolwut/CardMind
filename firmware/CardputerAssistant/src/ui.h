#pragma once

#include "app_types.h"

#include <M5Cardputer.h>

namespace cardputer {

enum class CarouselIcon {
    Chats,
    Ai,
    Voice,
    Network,
    Files,
    Device,
    Help,
};

enum class CarouselDirection {
    Previous,
    Next,
};

struct CarouselCard {
    String title;
    String subtitle;
    std::uint16_t accentColor;
    CarouselIcon icon;
};

OperationResult beginUi();
void showFatalError(const String& error);
void showProvisioning(const String& accessPointName, const String& accessPointPassword);
void showFilesPortal(const String& accessPointName, const String& accessPointPassword);
void showChat(const std::vector<Message>& history,
              const std::string& activeResponse,
              const std::string& input,
              KeyboardLayout layout,
              const String& chatTitle,
              const String& status,
              std::size_t scrollOffset,
              bool wifiConnected,
              int batteryLevel,
              bool batteryCharging);
void showCarousel(const std::vector<CarouselCard>& cards,
                  std::size_t selectedIndex,
                  bool wifiConnected,
                  bool sdReady,
                  int batteryLevel,
                  bool batteryCharging,
                  const String& status);
void animateCarousel(const std::vector<CarouselCard>& cards,
                     std::size_t previousIndex,
                     std::size_t selectedIndex,
                     CarouselDirection direction,
                     bool wifiConnected,
                     bool sdReady,
                     int batteryLevel,
                     bool batteryCharging,
                     const String& status);
std::size_t maximumChatScrollOffset(const std::vector<Message>& history,
                                    const std::string& activeResponse,
                                    const String& status);
void showSelectionList(const String& title,
                       const std::vector<String>& items,
                       std::size_t selectedIndex,
                       const String& footer);
void showTextViewer(const String& title,
                    const std::vector<std::string>& lines,
                    std::size_t firstLine,
                    const String& position);
void showTextEditor(const String& title,
                    const std::string& input,
                    KeyboardLayout layout,
                    std::size_t maximumBytes,
                    const String& status);
void showPasswordEntry(const String& ssid, std::size_t passwordLength, const String& status);
void showBusyScreen(const String& title, const String& message);
void showConfirmation(const String& title, const String& message, const String& footer);
void showVoiceRecording(std::uint32_t elapsedMs,
                        std::uint32_t maximumMs,
                        std::uint16_t level);
bool fontSupportsCyrillic();

}  // namespace cardputer
