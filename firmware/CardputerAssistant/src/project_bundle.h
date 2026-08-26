#pragma once

#include "app_types.h"

#include <limits>

namespace cardputer {

constexpr std::uint64_t kMaximumProjectBundleBytes =
    std::numeric_limits<std::uint32_t>::max();

OperationResult exportProjectBundle(const String& projectId, const String& filename);
ProjectDocumentResult importProjectBundle(const String& filename);

}  // namespace cardputer
