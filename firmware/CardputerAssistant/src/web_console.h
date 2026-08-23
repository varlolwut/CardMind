#pragma once

#include "app_types.h"

namespace cardputer {

struct WebConsoleResult {
    bool success;
    String activeChatId;
    String error;
};

WebConsoleResult runWebConsole(const Settings& settings, const String& initialChatId,
                               const String& version);

}  // namespace cardputer
