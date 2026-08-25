#pragma once

#include "app_types.h"

namespace cardputer {

constexpr std::uint64_t kMaximumProjectBundleBytes = 268435456;

OperationResult exportProjectBundle(const String& projectId, const String& filename);
ProjectDocumentResult importProjectBundle(const String& filename);

}  // namespace cardputer
