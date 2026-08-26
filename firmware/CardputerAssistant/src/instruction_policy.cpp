#include "instruction_policy.h"

namespace cardputer {
namespace {

void appendInstructionSection(std::string& result,
                              const char* heading,
                              const std::string& content)
{
    if (content.empty()) {
        return;
    }
    if (!result.empty()) {
        result += "\n\n";
    }
    result += heading;
    result += content;
}

}  // namespace

std::string buildScopedInstructions(const std::string& projectInstructions,
                                    const std::string& chatInstructions,
                                    const std::string& requestInstructions,
                                    const std::string& contextSummary)
{
    std::string result;
    appendInstructionSection(result,
                             "Project instructions supplied by the user:\n",
                             projectInstructions);
    appendInstructionSection(
        result,
        "Chat-specific instructions override conflicting project instructions:\n",
        chatInstructions);
    appendInstructionSection(
        result,
        "Conversation summary for turns omitted from the active context:\n",
        contextSummary);
    appendInstructionSection(
        result,
        "Instructions for this request override conflicting chat instructions:\n",
        requestInstructions);
    return result;
}

std::string buildUserInstructionScopes(const std::string& globalInstructions,
                                       const std::string& scopedInstructions)
{
    std::string result;
    appendInstructionSection(result,
                             "Global instructions supplied by the user:\n",
                             globalInstructions);
    appendInstructionSection(
        result,
        "Scoped instructions override conflicting global instructions:\n",
        scopedInstructions);
    return result;
}

}  // namespace cardputer
