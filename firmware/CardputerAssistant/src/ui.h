#pragma once

#include "app_types.h"

#include <M5Cardputer.h>

namespace cardputer {

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
              bool wifiConnected);
std::size_t maximumChatScrollOffset(const std::vector<Message>& history,
                                    const std::string& activeResponse,
                                    const String& status);
void showSelectionList(const String& title,
                       const std::vector<String>& items,
                       std::size_t selectedIndex,
                       const String& footer);
void showPasswordEntry(const String& ssid, std::size_t passwordLength, const String& status);
void showBusyScreen(const String& title, const String& message);
void showConfirmation(const String& title, const String& message, const String& footer);
void showVoiceRecording(std::uint32_t elapsedMs,
                        std::uint32_t maximumMs,
                        std::uint16_t level);
bool fontSupportsCyrillic();

}  // namespace cardputer
