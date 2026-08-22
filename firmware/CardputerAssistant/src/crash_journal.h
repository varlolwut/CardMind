#pragma once

#include <Arduino.h>

#include "app_types.h"

namespace cardputer {

void markOperation(const char* operation);
String previousOperation();
OperationResult appendBootJournal(const char* firmwareVersion);

}  // namespace cardputer
