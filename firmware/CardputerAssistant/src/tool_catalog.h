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
    SftpList = 7,
    SftpRead = 8,
    SftpWrite = 9,
    SftpMove = 10,
    SshSafeAction = 11,
    PythonRun = 12,
    Count = 13,
};

enum class ToolConfirmationReason : std::uint8_t {
    None,
    PolicyAsk,
    Mandatory,
};

constexpr std::size_t kToolCatalogSize =
    static_cast<std::size_t>(ToolSchemaId::Count);
using ToolSchemaMask = std::uint16_t;

struct ToolCatalogEntry {
    ToolSchemaId schema;
    const char* name;
    ToolCapabilityGroup group;
    ToolCapability capability;
};

struct SshSafeActionEntry {
    const char* id;
    const char* command;
};

constexpr std::size_t kSshSafeActionCount = 5;

struct ToolRequestPlan {
    ToolMessageIntent intent;
    std::array<ToolPermissionDecision, kToolCatalogSize> decisions;
    ToolSchemaMask schemas;
    std::uint8_t includedGroups;
    std::uint8_t missingRequiredGroups;
    ToolPolicyContractError error;
};

const std::array<ToolCatalogEntry, kToolCatalogSize>& toolCatalog() noexcept;
const std::array<SshSafeActionEntry, kSshSafeActionCount>&
sshSafeActionCatalog() noexcept;
const SshSafeActionEntry* sshSafeActionEntryForId(
    const std::string& id) noexcept;
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
    bool mutatesExistingTarget) noexcept;
std::uint8_t remainingRequiredGroupsAfterToolCall(
    const ToolRequestPlan& plan,
    std::uint8_t remainingGroups,
    const std::string& name) noexcept;

}  // namespace cardputer
