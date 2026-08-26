#pragma once

#include <cstddef>
#include <string>

namespace cardputer {

constexpr std::size_t kMaximumRequestInstructionsBytes = 2048;

std::string buildScopedInstructions(const std::string& projectInstructions,
                                    const std::string& chatInstructions,
                                    const std::string& requestInstructions,
                                    const std::string& contextSummary);
std::string buildUserInstructionScopes(const std::string& globalInstructions,
                                       const std::string& scopedInstructions);

}  // namespace cardputer
