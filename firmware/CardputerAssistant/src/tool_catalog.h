#pragma once

#include "tool_policy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

enum class ToolSchemaId : std::uint8_t {
    WebSearch = 0,
    WebFetch = 1,
    ListFiles = 2,
    ReadFile = 3,
    WriteFile = 4,
    AppendFile = 5,
    SshCommand = 6,
    Count = 7,
};

enum class ToolConfirmationReason : std::uint8_t {
    None,
    PolicyAsk,
    Mandatory,
};

constexpr std::size_t kToolCatalogSize =
    static_cast<std::size_t>(ToolSchemaId::Count);
using ToolSchemaMask = std::uint8_t;

struct ToolCatalogEntry {
    ToolSchemaId schema;
    const char* name;
    ToolCapabilityGroup group;
    ToolCapability capability;
};

struct ToolRequestPlan {
    ToolMessageIntent intent;
    std::array<ToolPermissionDecision, kToolCatalogSize> decisions;
    ToolSchemaMask schemas;
    std::uint8_t includedGroups;
    std::uint8_t missingRequiredGroups;
    ToolPolicyContractError error;
};

const std::array<ToolCatalogEntry, kToolCatalogSize>& toolCatalog() noexcept;
const ToolCatalogEntry* toolCatalogEntryForName(
    const std::string& name) noexcept;
ToolRequestPlan buildToolRequestPlan(
    const ToolPolicyResolutionResult& resolution,
    const ToolMessageIntent& intent) noexcept;
bool toolRequestPlanIsConsistent(const ToolRequestPlan& plan) noexcept;
bool toolRequestPlanIncludesSchema(
    const ToolRequestPlan& plan,
    ToolSchemaId schema) noexcept;
ToolPermissionDecision toolRequestPlanDecision(
    const ToolRequestPlan& plan,
    ToolSchemaId schema) noexcept;
ToolConfirmationReason toolConfirmationReason(
    ToolPermissionDecision decision,
    ToolSchemaId schema,
    bool replacesExistingFile) noexcept;
std::uint8_t remainingRequiredGroupsAfterToolCall(
    const ToolRequestPlan& plan,
    std::uint8_t remainingGroups,
    const std::string& name) noexcept;

}  // namespace cardputer
