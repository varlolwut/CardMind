#include "tool_catalog.h"

namespace cardputer {
namespace {

constexpr std::array<ToolCatalogEntry, kToolCatalogSize> kToolCatalog = {{
    {ToolSchemaId::WebSearch, "web_search", ToolCapabilityGroup::Web,
     ToolCapability::WebSearch},
    {ToolSchemaId::WebFetch, "web_fetch", ToolCapabilityGroup::Web,
     ToolCapability::WebFetch},
    {ToolSchemaId::ListFiles, "list_files", ToolCapabilityGroup::Files,
     ToolCapability::FilesRead},
    {ToolSchemaId::ReadFile, "read_file", ToolCapabilityGroup::Files,
     ToolCapability::FilesRead},
    {ToolSchemaId::WriteFile, "write_file", ToolCapabilityGroup::Files,
     ToolCapability::FilesWriteDelete},
    {ToolSchemaId::AppendFile, "append_file", ToolCapabilityGroup::Files,
     ToolCapability::FilesWriteDelete},
    {ToolSchemaId::SshCommand, "ssh_command", ToolCapabilityGroup::Ssh,
     ToolCapability::SshMutate},
    {ToolSchemaId::SftpList, "sftp_list", ToolCapabilityGroup::Ssh,
     ToolCapability::SftpReadWrite},
    {ToolSchemaId::SftpRead, "sftp_read", ToolCapabilityGroup::Ssh,
     ToolCapability::SftpReadWrite},
    {ToolSchemaId::SftpWrite, "sftp_write", ToolCapabilityGroup::Ssh,
     ToolCapability::SftpReadWrite},
    {ToolSchemaId::SftpMove, "sftp_move", ToolCapabilityGroup::Ssh,
     ToolCapability::SftpReadWrite},
    {ToolSchemaId::SshSafeAction, "ssh_safe_action",
     ToolCapabilityGroup::Ssh, ToolCapability::SshRead},
    {ToolSchemaId::PythonRun, "python_run",
     ToolCapabilityGroup::Python, ToolCapability::PythonWriteRun},
}};

constexpr std::array<SshSafeActionEntry, kSshSafeActionCount>
    kSshSafeActionCatalog = {{
        {"logs", "journalctl --no-pager --lines=100 --output=short-iso"},
        {"service_state",
         "systemctl list-units --type=service --state=running,failed "
         "--no-pager --plain"},
        {"containers", "docker ps --no-trunc"},
        {"disk", "df -hP"},
        {"processes", "ps -eo pid,ppid,user,stat,etime,comm"},
    }};

bool decisionIncludesSchema(ToolPermissionDecision decision) noexcept
{
    return decision == ToolPermissionDecision::Ask ||
           decision == ToolPermissionDecision::Allow;
}

std::uint8_t groupMask(ToolCapabilityGroup group) noexcept
{
    return static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(group));
}

}  // namespace

const std::array<ToolCatalogEntry, kToolCatalogSize>& toolCatalog() noexcept
{
    return kToolCatalog;
}

const std::array<SshSafeActionEntry, kSshSafeActionCount>&
sshSafeActionCatalog() noexcept
{
    return kSshSafeActionCatalog;
}

const SshSafeActionEntry* sshSafeActionEntryForId(
    const std::string& id) noexcept
{
    for (const SshSafeActionEntry& entry : kSshSafeActionCatalog) {
        if (id == entry.id) {
            return &entry;
        }
    }
    return nullptr;
}

const ToolCatalogEntry* toolCatalogEntryForName(
    const std::string& name) noexcept
{
    for (const ToolCatalogEntry& entry : kToolCatalog) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

ToolRequestPlan buildToolRequestPlan(
    const ToolPolicyResolutionResult& resolution,
    const ToolMessageIntent& intent) noexcept
{
    ToolRequestPlan plan = {};
    plan.intent = intent;
    plan.error = resolution.error;
    if (plan.error != ToolPolicyContractError::None) {
        return plan;
    }
    const bool intentModeIsValid =
        static_cast<std::uint8_t>(intent.mode) <
        static_cast<std::uint8_t>(ToolMessageIntentMode::Count);
    if (!intentModeIsValid) {
        plan.error = ToolPolicyContractError::InvalidIntentMode;
        return plan;
    }
    const bool groupsAreKnown =
        (intent.requiredGroups &
         static_cast<std::uint8_t>(~kAllToolCapabilityGroups)) == 0;
    const bool groupsMatchMode =
        intent.mode == ToolMessageIntentMode::Required
        ? intent.requiredGroups != 0
        : intent.requiredGroups == 0;
    if (!groupsAreKnown || !groupsMatchMode ||
        resolution.requiredGroups != intent.requiredGroups) {
        plan.error = ToolPolicyContractError::InvalidIntentGroups;
        return plan;
    }

    for (std::size_t index = 0; index < kToolCatalog.size(); ++index) {
        const ToolCatalogEntry& entry = kToolCatalog[index];
        const ToolPermissionDecision decision =
            resolution.permissions[static_cast<std::size_t>(
                entry.capability)].decision;
        plan.decisions[index] = decision;
        if (decisionIncludesSchema(decision)) {
            plan.schemas = static_cast<ToolSchemaMask>(
                plan.schemas | static_cast<ToolSchemaMask>(1U << index));
            plan.includedGroups = static_cast<std::uint8_t>(
                plan.includedGroups | groupMask(entry.group));
        }
    }
    plan.missingRequiredGroups = static_cast<std::uint8_t>(
        intent.requiredGroups &
        static_cast<std::uint8_t>(~plan.includedGroups));
    return plan;
}

bool toolRequestPlanIsConsistent(const ToolRequestPlan& plan) noexcept
{
    if (plan.error != ToolPolicyContractError::None ||
        static_cast<std::uint8_t>(plan.intent.mode) >=
            static_cast<std::uint8_t>(ToolMessageIntentMode::Count) ||
        (plan.intent.requiredGroups &
         static_cast<std::uint8_t>(~kAllToolCapabilityGroups)) != 0) {
        return false;
    }
    const bool groupsMatchMode =
        plan.intent.mode == ToolMessageIntentMode::Required
        ? plan.intent.requiredGroups != 0
        : plan.intent.requiredGroups == 0;
    if (!groupsMatchMode) {
        return false;
    }

    ToolSchemaMask schemas = 0;
    std::uint8_t includedGroups = 0;
    for (std::size_t index = 0; index < kToolCatalog.size(); ++index) {
        const ToolPermissionDecision decision = plan.decisions[index];
        if (static_cast<std::uint8_t>(decision) >
            static_cast<std::uint8_t>(ToolPermissionDecision::Unavailable)) {
            return false;
        }
        if (decisionIncludesSchema(decision)) {
            schemas = static_cast<ToolSchemaMask>(
                schemas | static_cast<ToolSchemaMask>(1U << index));
            includedGroups = static_cast<std::uint8_t>(
                includedGroups | groupMask(kToolCatalog[index].group));
        }
    }
    const std::uint8_t missingRequiredGroups = static_cast<std::uint8_t>(
        plan.intent.requiredGroups &
        static_cast<std::uint8_t>(~includedGroups));
    return plan.schemas == schemas &&
        plan.includedGroups == includedGroups &&
        plan.missingRequiredGroups == missingRequiredGroups;
}

bool toolRequestPlanIncludesSchema(
    const ToolRequestPlan& plan,
    ToolSchemaId schema) noexcept
{
    const std::uint8_t index = static_cast<std::uint8_t>(schema);
    return index < static_cast<std::uint8_t>(ToolSchemaId::Count) &&
        (plan.schemas & static_cast<ToolSchemaMask>(1U << index)) != 0;
}

ToolPermissionDecision toolRequestPlanDecision(
    const ToolRequestPlan& plan,
    ToolSchemaId schema) noexcept
{
    const std::uint8_t index = static_cast<std::uint8_t>(schema);
    return index < static_cast<std::uint8_t>(ToolSchemaId::Count)
        ? plan.decisions[index]
        : ToolPermissionDecision::Deny;
}

ToolConfirmationReason toolConfirmationReason(
    ToolPermissionDecision decision,
    ToolSchemaId schema,
    bool mutatesExistingTarget) noexcept
{
    if (static_cast<std::uint8_t>(schema) >=
            static_cast<std::uint8_t>(ToolSchemaId::Count) ||
        (decision != ToolPermissionDecision::Ask &&
         decision != ToolPermissionDecision::Allow)) {
        return ToolConfirmationReason::None;
    }
    if (schema == ToolSchemaId::SshCommand ||
        schema == ToolSchemaId::PythonRun ||
        ((schema == ToolSchemaId::WriteFile ||
          schema == ToolSchemaId::SftpWrite ||
          schema == ToolSchemaId::SftpMove) && mutatesExistingTarget)) {
        return ToolConfirmationReason::Mandatory;
    }
    return decision == ToolPermissionDecision::Ask
        ? ToolConfirmationReason::PolicyAsk
        : ToolConfirmationReason::None;
}

std::uint8_t remainingRequiredGroupsAfterToolCall(
    const ToolRequestPlan& plan,
    std::uint8_t remainingGroups,
    const std::string& name) noexcept
{
    if (!toolRequestPlanIsConsistent(plan)) {
        return remainingGroups;
    }
    const ToolCatalogEntry* entry = toolCatalogEntryForName(name);
    if (entry == nullptr ||
        !toolRequestPlanIncludesSchema(plan, entry->schema) ||
        !decisionIncludesSchema(toolRequestPlanDecision(plan, entry->schema))) {
        return remainingGroups;
    }
    return static_cast<std::uint8_t>(
        remainingGroups & static_cast<std::uint8_t>(~groupMask(entry->group)));
}

}  // namespace cardputer
