#pragma once

#include "app_types.h"

#include <M5Cardputer.h>

namespace cardputer {

constexpr std::size_t kMaximumQrPayloadBytes = 320;

enum class CarouselIcon {
    Chats,
    Ai,
    Voice,
    Network,
    Web,
    Files,
    Device,
    Tools,
    Help,
};

enum class CarouselDirection {
    Previous,
    Next,
};

struct CarouselCard {
    String kicker;
    String title;
    String subtitle;
    std::uint16_t accentColor;
    CarouselIcon icon;
};

struct DeviceDiagnosticsView {
    String firmware;
    String battery;
    String wifi;
    String storage;
    String heap;
    String largestHeap;
    String stack;
    String cpu;
    String uptime;
    String chats;
    String resetReason;
    String previousOperation;
    bool wifiConnected;
    bool storageReady;
    bool crashJournalReady;
    bool sshStorageReady;
};

OperationResult beginUi();
void showFatalError(const String& error);
void showProvisioning(const String& accessPointName, const String& accessPointPassword);
void showFilesPortal(const String& accessPointName, const String& accessPointPassword);
void showWebConsoleAccess(const String& address,
                          const String& accessPassword,
                          bool sessionActive,
                          bool passwordVisible);
void showPythonWorkspaceAccess(const String& address, const String& accessPassword);
void showPythonWorkspaceRunning(const String& address, const String& accessPassword);
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
void updateChatInput(const std::string& input);
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
void showDeviceDiagnostics(const DeviceDiagnosticsView& diagnostics, std::size_t pageIndex);
void showTextViewer(const String& title,
                    const std::vector<std::string>& lines,
                    std::size_t firstLine,
                    const String& position);
void showTextEditor(const String& title,
                    const std::string& input,
                    KeyboardLayout layout,
                    std::size_t maximumBytes,
                    const String& status,
                    const String& emptyHint,
                    const String& footer);
void showQrCode(const String& title, const String& payload, const String& footer);
void showFileEditor(const String& title,
                    const std::string& input,
                    std::size_t cursor,
                    KeyboardLayout layout,
                    std::size_t maximumBytes,
                    const String& position,
                    const String& status);
void showFilenameEntry(const String& title,
                       const std::string& input,
                       const String& status);
void showPasswordEntry(const String& ssid, std::size_t passwordLength, const String& status);
void showSecretEntry(const String& title, const String& label,
                     std::size_t secretLength, const String& status,
                     const String& footer);
void showBusyScreen(const String& title, const String& message);
void showConfirmation(const String& title, const String& message, const String& footer);
void showVoiceRecording(std::uint32_t elapsedMs,
                        std::uint32_t maximumMs,
                        std::uint16_t level);
bool fontSupportsCyrillic();

}  // namespace cardputer
