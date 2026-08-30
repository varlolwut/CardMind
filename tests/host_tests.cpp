#include "../firmware/CardputerAssistant/src/text_utils.h"
#include "../firmware/CardputerAssistant/src/audio_utils.h"
#include "../firmware/CardputerAssistant/src/document_reader.h"
#include "../firmware/CardputerAssistant/src/instruction_policy.h"
#include "../firmware/CardputerAssistant/src/json_string_reader.h"
#include "../firmware/CardputerAssistant/src/offline_tools.h"
#include "../firmware/CardputerAssistant/src/pending_tool_preview.h"
#include "../firmware/CardputerAssistant/src/ssh_terminal.h"
#include "../firmware/CardputerAssistant/src/tool_catalog.h"
#include "../firmware/CardputerAssistant/src/tool_policy.h"
#include "../firmware/CardputerAssistant/src/tool_policy_codec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

class MemoryJsonReader {
public:
    explicit MemoryJsonReader(const std::string& value) : value_(value), position_(0) {}

    int available() const
    {
        return position_ < value_.size() ? 1 : 0;
    }

    int read()
    {
        return available() ? static_cast<unsigned char>(value_[position_++]) : -1;
    }

    int peek() const
    {
        return available() ? static_cast<unsigned char>(value_[position_]) : -1;
    }

    std::size_t position() const
    {
        return position_;
    }

    bool seek(std::size_t position)
    {
        if (position > value_.size()) return false;
        position_ = position;
        return true;
    }

private:
    const std::string& value_;
    std::size_t position_;
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

cardputer::ToolPermissionPolicy uniformToolPermissionPolicy(
    cardputer::ToolPermission permission)
{
    cardputer::ToolPermissionPolicy result;
    result.fill(permission);
    return result;
}

cardputer::ScopedToolPermissionPolicy uniformScopedToolPermissionPolicy(
    cardputer::ScopedToolPermission permission)
{
    cardputer::ScopedToolPermissionPolicy result;
    result.fill(permission);
    return result;
}

cardputer::ToolAvailabilitySet uniformToolAvailability(bool available)
{
    cardputer::ToolAvailabilitySet result;
    result.fill(available);
    return result;
}

std::uint8_t toolCapabilityGroupMask(cardputer::ToolCapabilityGroup group)
{
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(group));
}

void requireInvalidToolResolution(
    const cardputer::ToolPolicyResolutionResult& result,
    cardputer::ToolPolicyContractError error)
{
    require(result.error == error && result.requiredGroups == 0,
            "Invalid tool policy input returned the wrong contract error");
    for (const cardputer::ResolvedToolPermission& permission : result.permissions) {
        require(permission.decision == cardputer::ToolPermissionDecision::Deny &&
                    permission.source == cardputer::ToolPermissionSource::None,
                "Invalid tool policy input did not fail closed");
    }
}

void testToolPolicyContracts()
{
    static_assert(std::is_trivially_copyable<
        cardputer::ToolPolicyResolutionResult>::value);

    require(cardputer::kToolCapabilityCount == 8 &&
                cardputer::kToolCapabilityGroupCount == 4,
            "Tool policy identity counts changed");
    const std::array<std::uint8_t, cardputer::kToolCapabilityCount>
        expectedMasks = {
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Files),
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Files),
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Ssh),
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Ssh),
            0,
            toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Python),
        };
    for (std::size_t index = 0; index < expectedMasks.size(); ++index) {
        const auto result = cardputer::toolCapabilityGroupMask(
            static_cast<cardputer::ToolCapability>(index));
        require(result.error == cardputer::ToolPolicyContractError::None &&
                    result.mask == expectedMasks[index],
                "Tool capability group mapping is incorrect");
    }

    const std::array<cardputer::ToolCapability, 2> invalidCapabilities = {
        cardputer::ToolCapability::Count,
        static_cast<cardputer::ToolCapability>(255),
    };
    for (const auto capability : invalidCapabilities) {
        const auto result = cardputer::toolCapabilityGroupMask(capability);
        require(result.error ==
                    cardputer::ToolPolicyContractError::InvalidCapability &&
                    result.mask == 0,
                "Invalid tool capability was accepted");
    }
}

void testToolMessageIntent()
{
    const auto builtIn = uniformToolPermissionPolicy(
        cardputer::ToolPermission::Allow);
    const auto global = builtIn;
    const auto project = uniformScopedToolPermissionPolicy(
        cardputer::ScopedToolPermission::Inherit);
    const auto chat = project;
    const auto available = uniformToolAvailability(true);
    const auto automatic = cardputer::resolveToolPolicy(
        builtIn, global, project, chat,
        {cardputer::ToolMessageIntentMode::Auto, 0}, available);
    require(automatic.error == cardputer::ToolPolicyContractError::None,
            "Automatic tool intent failed");

    for (std::uint8_t mask = 1;
         mask <= cardputer::kAllToolCapabilityGroups;
         ++mask) {
        const auto required = cardputer::resolveToolPolicy(
            builtIn, global, project, chat,
            {cardputer::ToolMessageIntentMode::Required, mask}, available);
        require(required.error == cardputer::ToolPolicyContractError::None &&
                    required.requiredGroups == mask,
                "Required tool intent lost its capability union");
        for (std::size_t index = 0;
             index < cardputer::kToolCapabilityCount;
             ++index) {
            require(required.permissions[index].decision ==
                        automatic.permissions[index].decision &&
                        required.permissions[index].source ==
                            automatic.permissions[index].source,
                    "Required tool intent changed an effective permission");
        }
    }

    struct InvalidIntentCase {
        cardputer::ToolMessageIntent intent;
        cardputer::ToolPolicyContractError error;
    };
    const std::array<InvalidIntentCase, 6> invalidCases = {{
        {{cardputer::ToolMessageIntentMode::Count, 0},
         cardputer::ToolPolicyContractError::InvalidIntentMode},
        {{static_cast<cardputer::ToolMessageIntentMode>(255), 0},
         cardputer::ToolPolicyContractError::InvalidIntentMode},
        {{cardputer::ToolMessageIntentMode::Required, 0},
         cardputer::ToolPolicyContractError::InvalidIntentGroups},
        {{cardputer::ToolMessageIntentMode::Auto, 1},
         cardputer::ToolPolicyContractError::InvalidIntentGroups},
        {{cardputer::ToolMessageIntentMode::NoTools, 1},
         cardputer::ToolPolicyContractError::InvalidIntentGroups},
        {{cardputer::ToolMessageIntentMode::Required, 0x10},
         cardputer::ToolPolicyContractError::InvalidIntentGroups},
    }};
    for (const InvalidIntentCase& invalidCase : invalidCases) {
        requireInvalidToolResolution(
            cardputer::resolveToolPolicy(
                builtIn, global, project, chat, invalidCase.intent, available),
            invalidCase.error);
    }
}

void testToolMessageIntentCodec()
{
    const std::array<cardputer::ToolMessageIntent, 2> simpleIntents = {{
        {cardputer::ToolMessageIntentMode::Auto, 0},
        {cardputer::ToolMessageIntentMode::NoTools, 0},
    }};
    const std::array<std::string, 2> simpleTexts = {"auto", "none"};
    for (std::size_t index = 0; index < simpleIntents.size(); ++index) {
        const auto encoded = cardputer::encodeToolMessageIntent(
            simpleIntents[index]);
        require(encoded.error ==
                    cardputer::ToolMessageIntentCodecError::None &&
                    std::string(encoded.value.data(), encoded.length) ==
                        simpleTexts[index],
                "Simple message intent encoded incorrectly");
        const auto decoded = cardputer::decodeToolMessageIntent(
            encoded.value.data(), encoded.length);
        require(decoded.error ==
                    cardputer::ToolMessageIntentCodecError::None &&
                    decoded.intent.mode == simpleIntents[index].mode &&
                    decoded.intent.requiredGroups == 0,
                "Simple message intent did not round-trip");
    }
    for (std::uint8_t mask = 1;
         mask <= cardputer::kAllToolCapabilityGroups;
         ++mask) {
        const auto encoded = cardputer::encodeToolMessageIntent(
            {cardputer::ToolMessageIntentMode::Required, mask});
        const char expectedDigit = mask < 10
            ? static_cast<char>('0' + mask)
            : static_cast<char>('a' + mask - 10);
        require(encoded.error ==
                    cardputer::ToolMessageIntentCodecError::None &&
                    encoded.length == 10 && encoded.value[9] == expectedDigit,
                "Required message intent was not canonical lowercase hex");
        const auto decoded = cardputer::decodeToolMessageIntent(
            encoded.value.data(), encoded.length);
        require(decoded.error ==
                    cardputer::ToolMessageIntentCodecError::None &&
                    decoded.intent.mode ==
                        cardputer::ToolMessageIntentMode::Required &&
                    decoded.intent.requiredGroups == mask,
                "Required message intent did not round-trip");
    }

    require(cardputer::encodeToolMessageIntent(
                {cardputer::ToolMessageIntentMode::Count, 0}).error ==
                cardputer::ToolMessageIntentCodecError::InvalidIntentMode &&
                cardputer::encodeToolMessageIntent(
                    {cardputer::ToolMessageIntentMode::Auto, 1}).error ==
                cardputer::ToolMessageIntentCodecError::InvalidIntentGroups &&
                cardputer::encodeToolMessageIntent(
                    {cardputer::ToolMessageIntentMode::Required, 0}).error ==
                cardputer::ToolMessageIntentCodecError::InvalidIntentGroups,
            "Invalid message intent encoded successfully");
    const std::array<std::string, 8> malformed = {
        "", "AUTO", "required:0", "required:A",
        "required:10", "required:g", "required:1x", " required:1",
    };
    for (const std::string& value : malformed) {
        require(cardputer::decodeToolMessageIntent(
                    value.data(), value.size()).error !=
                    cardputer::ToolMessageIntentCodecError::None,
                "Noncanonical message intent was accepted");
    }
    require(cardputer::decodeToolMessageIntent(nullptr, 4).error ==
                cardputer::ToolMessageIntentCodecError::InvalidLength,
            "Null message intent text was accepted");
}

cardputer::ToolPolicyResolutionResult uniformToolResolution(
    cardputer::ToolPermissionDecision decision)
{
    cardputer::ToolPolicyResolutionResult result = {};
    result.error = cardputer::ToolPolicyContractError::None;
    for (cardputer::ResolvedToolPermission& permission : result.permissions) {
        permission = {decision, cardputer::ToolPermissionSource::None};
    }
    return result;
}

void testToolCatalogAndRequestPlan()
{
    const auto& catalog = cardputer::toolCatalog();
    const std::array<std::string, 7> expectedNames = {
        "web_search", "web_fetch", "list_files", "read_file",
        "write_file", "append_file", "ssh_command",
    };
    const std::array<cardputer::ToolCapabilityGroup, 7> expectedGroups = {
        cardputer::ToolCapabilityGroup::Web,
        cardputer::ToolCapabilityGroup::Web,
        cardputer::ToolCapabilityGroup::Files,
        cardputer::ToolCapabilityGroup::Files,
        cardputer::ToolCapabilityGroup::Files,
        cardputer::ToolCapabilityGroup::Files,
        cardputer::ToolCapabilityGroup::Ssh,
    };
    const std::array<cardputer::ToolCapability, 7> expectedPrimary = {
        cardputer::ToolCapability::WebSearch,
        cardputer::ToolCapability::WebFetch,
        cardputer::ToolCapability::FilesRead,
        cardputer::ToolCapability::FilesRead,
        cardputer::ToolCapability::FilesWriteDelete,
        cardputer::ToolCapability::FilesWriteDelete,
        cardputer::ToolCapability::SshMutate,
    };
    require(catalog.size() == 7, "Tool catalog size changed");
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        require(static_cast<std::size_t>(catalog[index].schema) == index &&
                    expectedNames[index] == catalog[index].name &&
                    catalog[index].group == expectedGroups[index] &&
                    catalog[index].capability == expectedPrimary[index],
                "Tool catalog identity, mapping, or order changed");
        for (std::size_t other = index + 1;
             other < catalog.size(); ++other) {
            require(expectedNames[index] != catalog[other].name,
                    "Tool catalog contains a duplicate name");
        }
        require(cardputer::toolCatalogEntryForName(expectedNames[index]) ==
                    &catalog[index],
                "Canonical tool name did not resolve to its catalog row");
    }
    const std::array<std::string, 5> noncanonicalNames = {
        "WebSearch", "web-search", "SSH_COMMAND", "ssh-command", "sftp_list",
    };
    for (const std::string& name : noncanonicalNames) {
        require(cardputer::toolCatalogEntryForName(name) == nullptr,
                "Noncanonical or unsupported tool name entered the catalog");
    }

    auto resolution = uniformToolResolution(
        cardputer::ToolPermissionDecision::Deny);
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::WebSearch)].decision =
        cardputer::ToolPermissionDecision::Ask;
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::WebFetch)].decision =
        cardputer::ToolPermissionDecision::Allow;
    auto plan = cardputer::buildToolRequestPlan(
        resolution, {cardputer::ToolMessageIntentMode::Auto, 0});
    require(plan.error == cardputer::ToolPolicyContractError::None &&
                cardputer::toolRequestPlanIsConsistent(plan) &&
                plan.schemas == 0x03 &&
                plan.includedGroups == toolCapabilityGroupMask(
                    cardputer::ToolCapabilityGroup::Web) &&
                plan.missingRequiredGroups == 0 &&
                cardputer::toolRequestPlanDecision(
                    plan, cardputer::ToolSchemaId::WebSearch) ==
                    cardputer::ToolPermissionDecision::Ask &&
                cardputer::toolRequestPlanDecision(
                    plan, cardputer::ToolSchemaId::WebFetch) ==
                    cardputer::ToolPermissionDecision::Allow,
            "Ask/Allow Web schema selection is incorrect");

    resolution = uniformToolResolution(
        cardputer::ToolPermissionDecision::Unavailable);
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::SshMutate)].decision =
        cardputer::ToolPermissionDecision::Ask;
    plan = cardputer::buildToolRequestPlan(
        resolution, {cardputer::ToolMessageIntentMode::Auto, 0});
    require(plan.schemas == 0x40 &&
                plan.includedGroups == toolCapabilityGroupMask(
                    cardputer::ToolCapabilityGroup::Ssh),
            "SSH schema did not use conservative mutate eligibility");

    resolution = uniformToolResolution(
        cardputer::ToolPermissionDecision::Deny);
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::SshRead)].decision =
        cardputer::ToolPermissionDecision::Allow;
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::SshMutate)].decision =
        cardputer::ToolPermissionDecision::Unavailable;
    resolution.requiredGroups = toolCapabilityGroupMask(
        cardputer::ToolCapabilityGroup::Ssh);
    plan = cardputer::buildToolRequestPlan(
        resolution,
        {cardputer::ToolMessageIntentMode::Required,
         resolution.requiredGroups});
    require(plan.schemas == 0 &&
                plan.missingRequiredGroups == resolution.requiredGroups &&
                cardputer::toolRequestPlanDecision(
                    plan, cardputer::ToolSchemaId::SshCommand) ==
                    cardputer::ToolPermissionDecision::Unavailable,
            "SSH read-only policy exposed the arbitrary command schema");

    resolution = uniformToolResolution(
        cardputer::ToolPermissionDecision::Deny);
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::FilesRead)].decision =
        cardputer::ToolPermissionDecision::Ask;
    plan = cardputer::buildToolRequestPlan(
        resolution, {cardputer::ToolMessageIntentMode::Auto, 0});
    require(plan.schemas == 0x0C,
            "Files read permission exposed write schemas");
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::FilesRead)].decision =
        cardputer::ToolPermissionDecision::Unavailable;
    resolution.permissions[static_cast<std::size_t>(
        cardputer::ToolCapability::FilesWriteDelete)].decision =
        cardputer::ToolPermissionDecision::Allow;
    plan = cardputer::buildToolRequestPlan(
        resolution, {cardputer::ToolMessageIntentMode::Auto, 0});
    require(plan.schemas == 0x30,
            "Files write permission exposed read schemas");

    const auto builtIn = uniformToolPermissionPolicy(
        cardputer::ToolPermission::Allow);
    const auto scoped = uniformScopedToolPermissionPolicy(
        cardputer::ScopedToolPermission::Inherit);
    auto availability = uniformToolAvailability(true);
    availability[static_cast<std::size_t>(
        cardputer::ToolCapability::PythonWriteRun)] = false;
    for (std::uint8_t mask = 1;
         mask <= cardputer::kAllToolCapabilityGroups;
         ++mask) {
        const cardputer::ToolMessageIntent intent = {
            cardputer::ToolMessageIntentMode::Required, mask};
        const auto resolved = cardputer::resolveToolPolicy(
            builtIn, builtIn, scoped, scoped, intent, availability);
        const auto resolvedBefore = resolved;
        plan = cardputer::buildToolRequestPlan(resolved, intent);
        std::uint8_t remainingGroups = mask;
        for (const cardputer::ToolCatalogEntry& entry : catalog) {
            remainingGroups = cardputer::remainingRequiredGroupsAfterToolCall(
                plan, remainingGroups, entry.name);
        }
        const std::uint8_t pythonGroup = toolCapabilityGroupMask(
            cardputer::ToolCapabilityGroup::Python);
        bool resolutionUnchanged =
            resolved.requiredGroups == resolvedBefore.requiredGroups &&
            resolved.error == resolvedBefore.error;
        for (std::size_t index = 0;
             index < resolved.permissions.size(); ++index) {
            resolutionUnchanged = resolutionUnchanged &&
                resolved.permissions[index].decision ==
                    resolvedBefore.permissions[index].decision &&
                resolved.permissions[index].source ==
                    resolvedBefore.permissions[index].source;
        }
        require(plan.error == cardputer::ToolPolicyContractError::None &&
                    plan.schemas == 0x7F &&
                    plan.missingRequiredGroups ==
                        static_cast<std::uint8_t>(mask & pythonGroup) &&
                    remainingGroups ==
                        static_cast<std::uint8_t>(mask & pythonGroup) &&
                    resolutionUnchanged,
                "Required request plan lost obligations or changed input");
    }

    resolution = uniformToolResolution(
        cardputer::ToolPermissionDecision::Allow);
    const std::uint8_t requiredUnion = static_cast<std::uint8_t>(
        toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web) |
        toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Files) |
        toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Ssh));
    resolution.requiredGroups = requiredUnion;
    plan = cardputer::buildToolRequestPlan(
        resolution,
        {cardputer::ToolMessageIntentMode::Required, requiredUnion});
    std::uint8_t remaining = requiredUnion;
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        plan, remaining, "WebSearch");
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        plan, remaining, "sftp_list");
    require(remaining == requiredUnion,
            "Unknown or noncanonical tool call satisfied a required group");
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        plan, remaining, "web_fetch");
    require(remaining == static_cast<std::uint8_t>(
                requiredUnion & static_cast<std::uint8_t>(
                    ~toolCapabilityGroupMask(
                        cardputer::ToolCapabilityGroup::Web))),
            "Exact Web tool call did not satisfy only the Web group");
    cardputer::ToolRequestPlan omittedWrite = plan;
    omittedWrite.schemas = static_cast<cardputer::ToolSchemaMask>(
        omittedWrite.schemas & static_cast<cardputer::ToolSchemaMask>(
            ~(1U << static_cast<std::uint8_t>(
                cardputer::ToolSchemaId::WriteFile))));
    require(!cardputer::toolRequestPlanIsConsistent(omittedWrite),
            "Plan with an omitted Allow schema was accepted as consistent");
    const std::uint8_t beforeOmitted = remaining;
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        omittedWrite, remaining, "write_file");
    require(remaining == beforeOmitted,
            "Omitted schema satisfied a required group");
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        plan, remaining, "read_file");
    remaining = cardputer::remainingRequiredGroupsAfterToolCall(
        plan, remaining, "ssh_command");
    require(remaining == 0,
            "Exact multi-group tool calls left a required group unmatched");

    cardputer::ToolRequestPlan forged = plan;
    forged.decisions[static_cast<std::size_t>(
        cardputer::ToolSchemaId::WebSearch)] =
        cardputer::ToolPermissionDecision::Deny;
    require(!cardputer::toolRequestPlanIsConsistent(forged) &&
                cardputer::remainingRequiredGroupsAfterToolCall(
                forged,
                toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
                "web_search") ==
                toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
            "Inconsistent denied schema satisfied a required group");
    forged = plan;
    forged.schemas = static_cast<cardputer::ToolSchemaMask>(
        forged.schemas | 0x80U);
    require(!cardputer::toolRequestPlanIsConsistent(forged),
            "Plan with an unknown schema bit was accepted");
    forged = plan;
    forged.includedGroups = 0;
    require(!cardputer::toolRequestPlanIsConsistent(forged),
            "Plan with inconsistent included groups was accepted");
    forged = plan;
    forged.missingRequiredGroups = requiredUnion;
    require(!cardputer::toolRequestPlanIsConsistent(forged),
            "Plan with inconsistent missing groups was accepted");
    forged = plan;
    forged.intent.requiredGroups = cardputer::kAllToolCapabilityGroups + 1U;
    require(!cardputer::toolRequestPlanIsConsistent(forged),
            "Plan with invalid intent groups was accepted");
    forged = plan;
    forged.decisions[0] = static_cast<cardputer::ToolPermissionDecision>(255);
    require(!cardputer::toolRequestPlanIsConsistent(forged),
            "Plan with an invalid decision was accepted");
    forged.error = cardputer::ToolPolicyContractError::InvalidIntentGroups;
    require(cardputer::remainingRequiredGroupsAfterToolCall(
                forged,
                toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
                "web_fetch") ==
                toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Web),
            "Invalid request plan satisfied a required group");
    require(cardputer::toolRequestPlanDecision(
                plan, cardputer::ToolSchemaId::Count) ==
                cardputer::ToolPermissionDecision::Deny,
            "Invalid schema returned a permissive decision");

    for (const cardputer::ToolCatalogEntry& entry : catalog) {
        const bool alwaysMandatory =
            entry.schema == cardputer::ToolSchemaId::SshCommand;
        require(cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Deny,
                    entry.schema, true) ==
                    cardputer::ToolConfirmationReason::None &&
                    cardputer::toolConfirmationReason(
                        cardputer::ToolPermissionDecision::Unavailable,
                        entry.schema, true) ==
                        cardputer::ToolConfirmationReason::None,
                "Denied or unavailable schema requested confirmation");
        require(cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Allow,
                    entry.schema, false) ==
                    (alwaysMandatory
                         ? cardputer::ToolConfirmationReason::Mandatory
                         : cardputer::ToolConfirmationReason::None),
                "Allow confirmation matrix is incorrect");
        require(cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Ask,
                    entry.schema, false) ==
                    (alwaysMandatory
                         ? cardputer::ToolConfirmationReason::Mandatory
                         : cardputer::ToolConfirmationReason::PolicyAsk),
                "Ask confirmation matrix is incorrect");
    }
    require(cardputer::toolConfirmationReason(
                cardputer::ToolPermissionDecision::Allow,
                cardputer::ToolSchemaId::WriteFile, true) ==
                cardputer::ToolConfirmationReason::Mandatory &&
                cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Ask,
                    cardputer::ToolSchemaId::WriteFile, true) ==
                    cardputer::ToolConfirmationReason::Mandatory &&
                cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Allow,
                    cardputer::ToolSchemaId::AppendFile, true) ==
                    cardputer::ToolConfirmationReason::None,
            "Destructive file confirmation matrix is incorrect");
    require(cardputer::toolConfirmationReason(
                static_cast<cardputer::ToolPermissionDecision>(255),
                cardputer::ToolSchemaId::SshCommand, true) ==
                cardputer::ToolConfirmationReason::None &&
                cardputer::toolConfirmationReason(
                    cardputer::ToolPermissionDecision::Ask,
                    cardputer::ToolSchemaId::Count, true) ==
                cardputer::ToolConfirmationReason::None,
            "Invalid confirmation input did not fail closed");

    const auto noToolsResolution = cardputer::resolveToolPolicy(
        builtIn, builtIn, scoped, scoped,
        {cardputer::ToolMessageIntentMode::NoTools, 0},
        uniformToolAvailability(true));
    plan = cardputer::buildToolRequestPlan(
        noToolsResolution, {cardputer::ToolMessageIntentMode::NoTools, 0});
    require(plan.schemas == 0 && plan.includedGroups == 0 &&
                plan.missingRequiredGroups == 0,
            "No-tools intent retained a model schema");
}

void testPendingToolPreview()
{
    const auto ordinary = cardputer::buildPendingFileReplacementPreview(
        "alpha\nbeta\nomega\n", 17, true,
        "alpha\nBETA\nomega\n");
    require(ordinary.success && !ordinary.truncated &&
                ordinary.body.find("--- current (17 bytes)") !=
                    std::string::npos &&
                ordinary.body.find("+++ proposed (17 bytes)") !=
                    std::string::npos &&
                ordinary.body.find("@@ first difference at byte 6 @@") !=
                    std::string::npos &&
                ordinary.body.find("-beta\\n") != std::string::npos &&
                ordinary.body.find("+BETA\\n") != std::string::npos,
            "Ordinary file replacement preview is incorrect");

    const auto unchanged = cardputer::buildPendingFileReplacementPreview(
        "same\n", 5, true, "same\n");
    require(unchanged.success && !unchanged.truncated &&
                unchanged.body.find("@@ no changes @@") != std::string::npos,
            "Unchanged file replacement was not explicit");

    const auto whitespace = cardputer::buildPendingFileReplacementPreview(
        "alpha\r\n  beta \r\n", 16, true,
        "alpha\r\n beta\t\r\n");
    require(whitespace.success && !whitespace.truncated &&
                whitespace.body.find("-\\s\\sbeta\\s\\r\\n") !=
                    std::string::npos &&
                whitespace.body.find("+\\sbeta\\t\\r\\n") !=
                    std::string::npos,
            "Whitespace-only file replacement is not visible");

    const auto noFinalNewline = cardputer::buildPendingFileReplacementPreview(
        "line\nlast", 9, true, "line\nlast ");
    require(noFinalNewline.success && !noFinalNewline.truncated &&
                noFinalNewline.body.find("+last\\s\n") !=
                    std::string::npos,
            "No-final-newline replacement hid trailing whitespace");

    std::string deepCurrent(9001, 'a');
    std::string deepProposed = deepCurrent;
    deepCurrent[8999] = 'X';
    deepProposed[8999] = 'Y';
    deepCurrent.push_back('\n');
    deepProposed.push_back('\n');
    const auto deep = cardputer::buildPendingFileReplacementPreview(
        deepCurrent, static_cast<std::uint32_t>(deepCurrent.size()), true,
        deepProposed);
    require(deep.success && deep.truncated &&
                deep.body.find("first difference at byte 8999") !=
                    std::string::npos &&
                deep.body.find('X') != std::string::npos &&
                deep.body.find('Y') != std::string::npos,
            "Deep file change fell outside the bounded preview window");

    const std::string incompletePrefix(
        cardputer::kMaximumPendingFilePreviewSourceBytes, 'a');
    const auto incomplete = cardputer::buildPendingFileReplacementPreview(
        incompletePrefix, 15000, false, incompletePrefix);
    require(incomplete.success && incomplete.truncated &&
                incomplete.body.find("2712 current bytes not shown") !=
                    std::string::npos &&
                incomplete.body.size() <=
                    cardputer::kMaximumPendingToolPreviewBodyBytes,
            "Incomplete current-file prefix was presented as complete");

    const auto unicode = cardputer::buildPendingFileReplacementPreview(
        "Привет мир\n", 20, true, "Привет  мир\n");
    require(unicode.success && cardputer::isValidUtf8(unicode.body) &&
                unicode.body.size() <=
                    cardputer::kMaximumPendingToolPreviewBodyBytes,
            "UTF-8 file preview is invalid or unbounded");

    std::string clippedUtf8;
    for (std::size_t index = 0; index < 200; ++index) {
        clippedUtf8 += "я";
    }
    std::string changedUtf8 = clippedUtf8;
    changedUtf8.replace(0, std::string("я").size(), "ю");
    const auto utf8Boundary = cardputer::buildPendingFileReplacementPreview(
        clippedUtf8, static_cast<std::uint32_t>(clippedUtf8.size()), true,
        changedUtf8);
    require(utf8Boundary.success && utf8Boundary.truncated &&
                cardputer::isValidUtf8(utf8Boundary.body) &&
                utf8Boundary.body.find("first difference at byte 0") !=
                    std::string::npos,
            "UTF-8 line clipping split a code point boundary");

    std::string manyCurrent = "old\n";
    std::string manyProposed = "new\n";
    for (std::size_t index = 0; index < 20; ++index) {
        manyCurrent += "current line " + std::to_string(index) + "\n";
        manyProposed += "proposed line " + std::to_string(index) + "\n";
    }
    const auto bounded = cardputer::buildPendingFileReplacementPreview(
        manyCurrent, static_cast<std::uint32_t>(manyCurrent.size()), true,
        manyProposed);
    std::size_t lineCount = 0;
    std::size_t lineBytes = 0;
    std::size_t maximumLineBytes = 0;
    for (const char value : bounded.body) {
        if (value == '\n') {
            ++lineCount;
            maximumLineBytes = std::max(maximumLineBytes, lineBytes);
            lineBytes = 0;
        } else {
            ++lineBytes;
        }
    }
    require(bounded.success && bounded.truncated &&
                bounded.body.size() <=
                    cardputer::kMaximumPendingToolPreviewBodyBytes &&
                lineCount <= 13 &&
                maximumLineBytes <=
                    cardputer::kMaximumPendingToolPreviewLineBytes,
            "File preview exceeded its body, line, or line-width bound");

    const std::string command =
        "  printf '\"quoted\"\\\\path'\t\n echo Привет  ";
    const auto ssh = cardputer::buildPendingSshCommandPreview(command);
    require(ssh.success && !ssh.truncated && ssh.body == command,
            "SSH command preview changed exact command bytes");
    require(!cardputer::buildPendingSshCommandPreview("").success &&
                !cardputer::buildPendingSshCommandPreview(
                    std::string(1025, 'x')).success,
            "SSH command preview accepted input outside its exact limits");

    require(!cardputer::buildPendingFileReplacementPreview(
                 "x", 2, true, "y").success,
            "File preview accepted inconsistent completeness metadata");
}

void testToolPolicyPrecedence()
{
    const std::array<cardputer::ToolPermission, 3> permissions = {
        cardputer::ToolPermission::Off,
        cardputer::ToolPermission::Ask,
        cardputer::ToolPermission::Allow,
    };
    const std::array<cardputer::ScopedToolPermission, 4> scopedPermissions = {
        cardputer::ScopedToolPermission::Inherit,
        cardputer::ScopedToolPermission::Off,
        cardputer::ScopedToolPermission::Ask,
        cardputer::ScopedToolPermission::Allow,
    };
    const auto available = uniformToolAvailability(true);
    const cardputer::ToolMessageIntent automatic = {
        cardputer::ToolMessageIntentMode::Auto, 0};
    const std::size_t target = static_cast<std::size_t>(
        cardputer::ToolCapability::FilesWriteDelete);

    for (const auto builtInValue : permissions) {
        for (const auto globalValue : permissions) {
            for (const auto projectValue : scopedPermissions) {
                for (const auto chatValue : scopedPermissions) {
                    auto builtIn = uniformToolPermissionPolicy(
                        cardputer::ToolPermission::Allow);
                    auto global = builtIn;
                    auto project = uniformScopedToolPermissionPolicy(
                        cardputer::ScopedToolPermission::Inherit);
                    auto chat = project;
                    builtIn[target] = builtInValue;
                    global[target] = globalValue;
                    project[target] = projectValue;
                    chat[target] = chatValue;
                    const auto builtInBefore = builtIn;
                    const auto globalBefore = global;
                    const auto projectBefore = project;
                    const auto chatBefore = chat;

                    std::uint8_t expectedRank = 2;
                    auto expectedSource =
                        cardputer::ToolPermissionSource::None;
                    const auto restrictExpected =
                        [&expectedRank, &expectedSource](
                            std::uint8_t rank,
                            cardputer::ToolPermissionSource source) {
                            if (rank < expectedRank) {
                                expectedRank = rank;
                                expectedSource = source;
                            }
                        };
                    restrictExpected(
                        static_cast<std::uint8_t>(builtInValue),
                        cardputer::ToolPermissionSource::BuiltIn);
                    restrictExpected(
                        static_cast<std::uint8_t>(globalValue),
                        cardputer::ToolPermissionSource::Global);
                    if (projectValue !=
                        cardputer::ScopedToolPermission::Inherit) {
                        restrictExpected(
                            static_cast<std::uint8_t>(projectValue) - 1,
                            cardputer::ToolPermissionSource::Project);
                    }
                    if (chatValue !=
                        cardputer::ScopedToolPermission::Inherit) {
                        restrictExpected(
                            static_cast<std::uint8_t>(chatValue) - 1,
                            cardputer::ToolPermissionSource::Chat);
                    }

                    const auto result = cardputer::resolveToolPolicy(
                        builtIn, global, project, chat, automatic, available);
                    require(
                        result.error ==
                                cardputer::ToolPolicyContractError::None &&
                            result.permissions[target].decision ==
                                static_cast<
                                    cardputer::ToolPermissionDecision>(
                                    expectedRank) &&
                            result.permissions[target].source ==
                                expectedSource,
                        "Tool policy precedence is incorrect");
                    for (std::size_t index = 0;
                         index < result.permissions.size();
                         ++index) {
                        if (index != target) {
                            require(
                                result.permissions[index].decision ==
                                        cardputer::
                                            ToolPermissionDecision::Allow &&
                                    result.permissions[index].source ==
                                        cardputer::
                                            ToolPermissionSource::None,
                                "One capability changed another capability");
                        }
                    }
                    require(builtIn == builtInBefore &&
                                global == globalBefore &&
                                project == projectBefore &&
                                chat == chatBefore,
                            "Tool policy resolution modified an input");
                }
            }
        }
    }

    for (std::size_t denied = 0;
         denied < cardputer::kToolCapabilityCount;
         ++denied) {
        const auto builtIn = uniformToolPermissionPolicy(
            cardputer::ToolPermission::Allow);
        auto global = builtIn;
        global[denied] = cardputer::ToolPermission::Off;
        const auto project = uniformScopedToolPermissionPolicy(
            cardputer::ScopedToolPermission::Inherit);
        const auto result = cardputer::resolveToolPolicy(
            builtIn, global, project, project, automatic, available);
        for (std::size_t index = 0;
             index < result.permissions.size();
             ++index) {
            require(
                result.permissions[index].decision ==
                        (index == denied
                             ? cardputer::ToolPermissionDecision::Deny
                             : cardputer::ToolPermissionDecision::Allow) &&
                    result.permissions[index].source ==
                        (index == denied
                             ? cardputer::ToolPermissionSource::Global
                             : cardputer::ToolPermissionSource::None),
                "Capability identity isolation failed");
        }
    }
}

void testToolPolicyRoadmapExamples()
{
    const auto builtIn = uniformToolPermissionPolicy(
        cardputer::ToolPermission::Allow);
    auto global = builtIn;
    const auto project = uniformScopedToolPermissionPolicy(
        cardputer::ScopedToolPermission::Inherit);
    auto chat = project;
    const auto available = uniformToolAvailability(true);
    const cardputer::ToolMessageIntent automatic = {
        cardputer::ToolMessageIntentMode::Auto, 0};
    const std::size_t ssh = static_cast<std::size_t>(
        cardputer::ToolCapability::SshRead);
    const std::size_t files = static_cast<std::size_t>(
        cardputer::ToolCapability::FilesRead);
    const std::size_t web = static_cast<std::size_t>(
        cardputer::ToolCapability::WebSearch);

    global[ssh] = cardputer::ToolPermission::Off;
    chat[ssh] = cardputer::ScopedToolPermission::Allow;
    auto result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, available);
    require(
        result.permissions[ssh].decision ==
                cardputer::ToolPermissionDecision::Deny &&
            result.permissions[ssh].source ==
                cardputer::ToolPermissionSource::Global,
        "Chat Allow elevated global SSH Off");

    global[ssh] = cardputer::ToolPermission::Ask;
    result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, available);
    require(
        result.permissions[ssh].decision ==
                cardputer::ToolPermissionDecision::Ask &&
            result.permissions[ssh].source ==
                cardputer::ToolPermissionSource::Global,
        "Chat Allow elevated global SSH Ask");

    global[ssh] = cardputer::ToolPermission::Allow;
    chat[ssh] = cardputer::ScopedToolPermission::Inherit;
    chat[files] = cardputer::ScopedToolPermission::Off;
    result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, available);
    require(
        result.permissions[files].decision ==
                cardputer::ToolPermissionDecision::Deny &&
            result.permissions[files].source ==
                cardputer::ToolPermissionSource::Chat,
        "Chat Files Off did not restrict global Files Allow");

    result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, available);
    require(
        result.permissions[web].decision ==
                cardputer::ToolPermissionDecision::Allow &&
            result.permissions[web].source ==
                cardputer::ToolPermissionSource::None,
        "Inherited Web permission did not preserve global Allow");

    global[ssh] = cardputer::ToolPermission::Off;
    const std::uint8_t sshGroup =
        toolCapabilityGroupMask(cardputer::ToolCapabilityGroup::Ssh);
    result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat,
        {cardputer::ToolMessageIntentMode::Required, sshGroup}, available);
    require(result.requiredGroups == sshGroup &&
                result.permissions[ssh].decision ==
                    cardputer::ToolPermissionDecision::Deny,
            "Required SSH elevated a denied policy or lost its obligation");
}

void testToolPolicyIntentAvailabilityAndErrors()
{
    auto builtIn = uniformToolPermissionPolicy(
        cardputer::ToolPermission::Allow);
    auto global = builtIn;
    auto project = uniformScopedToolPermissionPolicy(
        cardputer::ScopedToolPermission::Inherit);
    auto chat = project;
    auto availability = uniformToolAvailability(true);
    const cardputer::ToolMessageIntent automatic = {
        cardputer::ToolMessageIntentMode::Auto, 0};

    const auto noTools = cardputer::resolveToolPolicy(
        builtIn, global, project, chat,
        {cardputer::ToolMessageIntentMode::NoTools, 0}, availability);
    for (const auto& permission : noTools.permissions) {
        require(
            permission.decision ==
                    cardputer::ToolPermissionDecision::Deny &&
                permission.source ==
                    cardputer::ToolPermissionSource::Message,
            "No tools left an executable capability");
    }

    const std::size_t python = static_cast<std::size_t>(
        cardputer::ToolCapability::PythonWriteRun);
    availability[python] = false;
    auto result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, availability);
    require(
        result.permissions[python].decision ==
                cardputer::ToolPermissionDecision::Unavailable &&
            result.permissions[python].source ==
                cardputer::ToolPermissionSource::Availability,
        "Unavailable Python capability was not explicit");
    global[python] = cardputer::ToolPermission::Off;
    result = cardputer::resolveToolPolicy(
        builtIn, global, project, chat, automatic, availability);
    require(
        result.permissions[python].decision ==
                cardputer::ToolPermissionDecision::Deny &&
            result.permissions[python].source ==
                cardputer::ToolPermissionSource::Global,
        "Capability unavailability hid an explicit policy denial");

    const std::array<cardputer::ToolPermission, 2> invalidPermissions = {
        cardputer::ToolPermission::Count,
        static_cast<cardputer::ToolPermission>(255),
    };
    const std::array<cardputer::ScopedToolPermission, 2>
        invalidScopedPermissions = {
            cardputer::ScopedToolPermission::Count,
            static_cast<cardputer::ScopedToolPermission>(255),
        };
    for (std::size_t index = 0;
         index < cardputer::kToolCapabilityCount;
         ++index) {
        for (const auto invalid : invalidPermissions) {
            builtIn = uniformToolPermissionPolicy(
                cardputer::ToolPermission::Allow);
            global = builtIn;
            project = uniformScopedToolPermissionPolicy(
                cardputer::ScopedToolPermission::Inherit);
            chat = project;
            builtIn[index] = invalid;
            requireInvalidToolResolution(
                cardputer::resolveToolPolicy(
                    builtIn, global, project, chat, automatic, availability),
                cardputer::ToolPolicyContractError::
                    InvalidBuiltInPermission);
            builtIn[index] = cardputer::ToolPermission::Allow;
            global[index] = invalid;
            requireInvalidToolResolution(
                cardputer::resolveToolPolicy(
                    builtIn, global, project, chat, automatic, availability),
                cardputer::ToolPolicyContractError::
                    InvalidGlobalPermission);
        }
        for (const auto invalid : invalidScopedPermissions) {
            global = uniformToolPermissionPolicy(
                cardputer::ToolPermission::Allow);
            project = uniformScopedToolPermissionPolicy(
                cardputer::ScopedToolPermission::Inherit);
            chat = project;
            project[index] = invalid;
            requireInvalidToolResolution(
                cardputer::resolveToolPolicy(
                    global, global, project, chat, automatic, availability),
                cardputer::ToolPolicyContractError::
                    InvalidProjectPermission);
            project[index] =
                cardputer::ScopedToolPermission::Inherit;
            chat[index] = invalid;
            requireInvalidToolResolution(
                cardputer::resolveToolPolicy(
                    global, global, project, chat, automatic, availability),
                cardputer::ToolPolicyContractError::
                    InvalidChatPermission);
        }
    }
}

void testToolPolicyCodecDefaultsAndLegacyMigration()
{
    const auto master = cardputer::defaultGlobalToolPermissionPolicy();
    const auto newChat = cardputer::defaultNewChatToolPermissionPolicy();
    const auto inherited = cardputer::inheritedToolPermissionPolicy();
    const auto encodedMaster = cardputer::encodeToolPermissionPolicy(master);
    const auto encodedNewChat =
        cardputer::encodeScopedToolPermissionPolicy(newChat);
    require(encodedMaster.error == cardputer::ToolPolicyCodecError::None &&
                std::string(encodedMaster.encoded.value.data()) ==
                    "v1;ws=a;wf=a;fr=a;fw=a;sr=a;sm=a;sf=a;py=o",
            "Default Master policy or persisted capability order changed");
    require(encodedNewChat.error == cardputer::ToolPolicyCodecError::None &&
                std::string(encodedNewChat.encoded.value.data()) ==
                    "v1;ws=a;wf=a;fr=q;fw=q;sr=o;sm=o;sf=o;py=o",
            "Default new-chat policy changed");
    for (const auto permission : inherited) {
        require(permission == cardputer::ScopedToolPermission::Inherit,
                "Inherited project policy contains an explicit permission");
    }

    const auto legacyDisabled =
        cardputer::migrateLegacyChatToolPermissionPolicy(false);
    const auto legacyEnabled =
        cardputer::migrateLegacyChatToolPermissionPolicy(true);
    require(!cardputer::legacySshToolsEnabled(legacyDisabled),
            "Disabled legacy SSH state migrated as enabled");
    require(cardputer::legacySshToolsEnabled(legacyEnabled),
            "Enabled legacy SSH state did not preserve its grant");
    auto mixedSsh = legacyEnabled;
    mixedSsh[static_cast<std::size_t>(cardputer::ToolCapability::SshMutate)] =
        cardputer::ScopedToolPermission::Ask;
    require(!cardputer::legacySshToolsEnabled(mixedSsh),
            "Lossy legacy SSH projection granted a mixed policy");
    const auto disabledAgain =
        cardputer::setLegacySshToolsEnabled(legacyEnabled, false);
    require(disabledAgain[static_cast<std::size_t>(
                cardputer::ToolCapability::WebSearch)] ==
                legacyEnabled[static_cast<std::size_t>(
                    cardputer::ToolCapability::WebSearch)] &&
                !cardputer::legacySshToolsEnabled(disabledAgain),
            "Legacy SSH update changed an unrelated capability");
}

void testToolPolicyCodecRoundTrip()
{
    for (std::size_t index = 0;
         index < cardputer::kToolCapabilityCount;
         ++index) {
        for (std::uint8_t value = 0;
             value < static_cast<std::uint8_t>(cardputer::ToolPermission::Count);
             ++value) {
            auto policy = cardputer::defaultGlobalToolPermissionPolicy();
            policy[index] = static_cast<cardputer::ToolPermission>(value);
            const auto encoded = cardputer::encodeToolPermissionPolicy(policy);
            require(encoded.error == cardputer::ToolPolicyCodecError::None,
                    "Valid Master policy failed to encode");
            const auto decoded = cardputer::decodeToolPermissionPolicy(
                encoded.encoded.value.data(), cardputer::kEncodedToolPolicyLength);
            require(decoded.error == cardputer::ToolPolicyCodecError::None &&
                        decoded.policy == policy,
                    "Master policy did not round-trip");
        }
        for (std::uint8_t value = 0;
             value < static_cast<std::uint8_t>(
                 cardputer::ScopedToolPermission::Count);
             ++value) {
            auto policy = cardputer::defaultNewChatToolPermissionPolicy();
            policy[index] = static_cast<cardputer::ScopedToolPermission>(value);
            const auto encoded =
                cardputer::encodeScopedToolPermissionPolicy(policy);
            require(encoded.error == cardputer::ToolPolicyCodecError::None,
                    "Valid scoped policy failed to encode");
            const auto decoded = cardputer::decodeScopedToolPermissionPolicy(
                encoded.encoded.value.data(), cardputer::kEncodedToolPolicyLength);
            require(decoded.error == cardputer::ToolPolicyCodecError::None &&
                        decoded.policy == policy,
                    "Scoped policy did not round-trip");
        }
    }

    auto invalidMaster = cardputer::defaultGlobalToolPermissionPolicy();
    invalidMaster[0] = static_cast<cardputer::ToolPermission>(255);
    require(cardputer::encodeToolPermissionPolicy(invalidMaster).error ==
                cardputer::ToolPolicyCodecError::InvalidPermission,
            "Invalid Master permission encoded successfully");
    auto invalidScoped = cardputer::defaultNewChatToolPermissionPolicy();
    invalidScoped[0] = static_cast<cardputer::ScopedToolPermission>(255);
    require(cardputer::encodeScopedToolPermissionPolicy(invalidScoped).error ==
                cardputer::ToolPolicyCodecError::InvalidPermission,
            "Invalid scoped permission encoded successfully");
}

void testToolPolicyCodecRejectsMalformedText()
{
    const auto encoded = cardputer::encodeScopedToolPermissionPolicy(
        cardputer::defaultNewChatToolPermissionPolicy());
    require(encoded.error == cardputer::ToolPolicyCodecError::None,
            "Scoped codec fixture failed to encode");
    const std::string canonical(encoded.encoded.value.data());
    require(cardputer::decodeScopedToolPermissionPolicy(
                nullptr, cardputer::kEncodedToolPolicyLength).error ==
                cardputer::ToolPolicyCodecError::InvalidLength,
            "Null policy text was accepted");
    require(cardputer::decodeScopedToolPermissionPolicy(
                canonical.c_str(), canonical.size() - 1).error ==
                cardputer::ToolPolicyCodecError::InvalidLength,
            "Truncated policy text was accepted");
    const std::string trailing = canonical + "x";
    require(cardputer::decodeScopedToolPermissionPolicy(
                trailing.c_str(), trailing.size()).error ==
                cardputer::ToolPolicyCodecError::InvalidLength,
            "Policy text with trailing data was accepted");

    std::string malformed = canonical;
    malformed[1] = '2';
    require(cardputer::decodeScopedToolPermissionPolicy(
                malformed.c_str(), malformed.size()).error ==
                cardputer::ToolPolicyCodecError::InvalidVersion,
            "Unknown policy version was accepted");
    malformed = canonical;
    malformed[3] = 'x';
    require(cardputer::decodeScopedToolPermissionPolicy(
                malformed.c_str(), malformed.size()).error ==
                cardputer::ToolPolicyCodecError::InvalidCapabilityOrder,
            "Unknown capability name was accepted");
    malformed = canonical;
    malformed[6] = 'x';
    require(cardputer::decodeScopedToolPermissionPolicy(
                malformed.c_str(), malformed.size()).error ==
                cardputer::ToolPolicyCodecError::InvalidPermissionCode,
            "Unknown scoped permission code was accepted");
    malformed[6] = 'i';
    require(cardputer::decodeToolPermissionPolicy(
                malformed.c_str(), malformed.size()).error ==
                cardputer::ToolPolicyCodecError::InvalidPermissionCode,
            "Inherit was accepted in a Master policy");
}

void testUtf8Backspace()
{
    require(cardputer::removeLastUtf8CodePoint("hello") == "hell", "ASCII backspace failed");
    require(cardputer::removeLastUtf8CodePoint("test я") == "test ", "Cyrillic backspace failed");
    require(cardputer::removeLastUtf8CodePoint("") == "", "Empty backspace failed");
    const std::string value = "AяB";
    require(cardputer::previousUtf8Boundary(value, 3) == 1,
            "UTF-8 previous cursor boundary failed");
    require(cardputer::nextUtf8Boundary(value, 1) == 3,
            "UTF-8 next cursor boundary failed");
    require(cardputer::insertUtf8At(value, 3, "!") == "Aя!B",
            "UTF-8 cursor insertion failed");
    require(cardputer::eraseUtf8Before(value, 3) == "AB",
            "UTF-8 cursor erasure failed");
}

void testRussianLayout()
{
    require(cardputer::mapKeyToRussian('q') == "й", "Lowercase Russian layout failed");
    require(cardputer::mapKeyToRussian('Q') == "Й", "Uppercase Russian layout failed");
    require(cardputer::mapKeyToRussian('`') == "ё", "Russian yo mapping failed");
    require(cardputer::mapKeyToRussian('1') == "1", "Numeric key mapping failed");
}

void testWrapping()
{
    const std::vector<std::string> expected = {"ab", "я", "cd"};
    require(cardputer::wrapUtf8Text("abя" "cd", 2) == expected, "UTF-8 wrapping failed");
    require(cardputer::wrapUtf8Text("a\nb", 10).size() == 2, "Newline wrapping failed");
    const std::vector<std::string> wordWrapped = {"one two", "three"};
    require(cardputer::wrapUtf8Text("one two three", 7) == wordWrapped,
            "Word-aware wrapping failed");
    const std::vector<std::string> russianWrapped = {"Ты: Это", "пример", "строки"};
    require(cardputer::wrapUtf8Text("Ты: Это пример строки", 14) == russianWrapped,
            "Cyrillic word-aware wrapping failed");
    const std::string largeUnbrokenText(65536, 'x');
    const auto largeWrapped = cardputer::wrapUtf8Text(largeUnbrokenText, 38);
    require(largeWrapped.size() == 1725, "Large text line count is incorrect");
    std::size_t wrappedBytes = 0;
    for (const auto& line : largeWrapped) {
        require(line.size() <= 38, "Large text line exceeds the requested width");
        wrappedBytes += line.size();
    }
    require(wrappedBytes == largeUnbrokenText.size(), "Large text wrapping lost data");
}

void testSse()
{
    std::string data;
    require(cardputer::extractSseData("data: {\"ok\":true}\r", data), "SSE data line not detected");
    require(data == "{\"ok\":true}", "SSE payload extraction failed");
    require(!cardputer::extractSseData("event: message", data), "Non-data SSE line detected");
    require(cardputer::buildVersionedApiUrl("https://api.example.com", "/v1/models") ==
                "https://api.example.com/v1/models",
            "Unversioned API base URL was joined incorrectly");
    require(cardputer::buildVersionedApiUrl("https://api.example.com/v1", "/v1/chat/completions") ==
                "https://api.example.com/v1/chat/completions",
            "Versioned API base URL duplicated /v1");
}

void testUtf8Validation()
{
    require(cardputer::isValidUtf8("Привет"), "Valid UTF-8 rejected");
    require(!cardputer::isValidUtf8(std::string("\xD0", 1)), "Truncated UTF-8 accepted");
    require(!cardputer::isValidUtf8(std::string("a\0b", 3)),
            "Embedded NUL accepted as application text");
    const std::string largeValid(65536, 'x');
    require(cardputer::isValidUtf8(largeValid), "Large valid UTF-8 rejected");
    std::string largeInvalid = largeValid;
    largeInvalid.push_back(static_cast<char>(0xF0));
    require(!cardputer::isValidUtf8(largeInvalid), "Large truncated UTF-8 accepted");
}

void testWavHeader()
{
    const auto header = cardputer::buildPcmWavHeader(16000, 16000);
    require(std::string(header.begin(), header.begin() + 4) == "RIFF", "WAV RIFF marker failed");
    require(std::string(header.begin() + 8, header.begin() + 12) == "WAVE", "WAV format marker failed");
    require(header[24] == 0x80 && header[25] == 0x3E, "WAV sample rate failed");
    require(header[40] == 0x00 && header[41] == 0x7D, "WAV data size failed");
}

void testChatText()
{
    require(cardputer::makeChatTitle("  Первый\n\tзапрос пользователя  ", 18) ==
                "Первый з...",
            "Cyrillic chat title generation failed");
    require(cardputer::ellipsizeUtf8("Коротко", 20) == "Коротко",
            "Short UTF-8 title changed");
    require(cardputer::isValidChatId("0123456789abcdef"), "Valid chat id rejected");
    require(!cardputer::isValidChatId("0123456789ABCDEF"), "Uppercase chat id accepted");
    require(cardputer::isValidWorkspaceFilename("notes_ru.md"), "Valid workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("automation.py"), "Python workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("chat_export.chat.jsonl"),
            "Portable chat bundle filename rejected");
    require(!cardputer::isValidWorkspaceFilename("../secret.txt"), "Traversal filename accepted");
    require(cardputer::isValidWorkspaceFilename("bin/program.exe"),
            "Arbitrary nested workspace file rejected");
    require(cardputer::isValidWorkspaceFilename(".hidden.txt"),
            "Hidden workspace filename rejected");
    require(cardputer::isValidWorkspaceFilename("note.MD"),
            "Uppercase workspace extension rejected");
    const std::vector<std::string> textExtensions = {
        ".txt", ".md", ".json", ".jsonl", ".csv", ".html", ".svg", ".py",
        ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf", ".log", ".xml",
        ".css", ".js", ".mjs", ".cjs", ".ts", ".tsx", ".sh", ".bash", ".zsh",
        ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", ".ino",
        ".env", ".properties", ".sql",
    };
    for (const std::string& extension : textExtensions) {
        require(cardputer::isWorkspaceTextFile("nested/source" + extension),
                "Safe workspace text extension rejected: " + extension);
        require(cardputer::requestsWorkspaceAccess("inspect nested/source" + extension),
                "Safe workspace text extension did not enable tools: " + extension);
        std::string uppercase = extension;
        for (char& value : uppercase) {
            value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        }
        require(cardputer::isWorkspaceTextFile("nested/source" + uppercase),
                "Uppercase workspace text extension rejected: " + uppercase);
        require(cardputer::requestsWorkspaceAccess("inspect nested/source" + uppercase),
                "Uppercase workspace text extension did not enable tools: " + uppercase);
    }
    const std::vector<std::string> transferOnlyNames = {
        "firmware.bin", "archive.zip", "program.exe", "image.png", "README",
        "notes.txt.exe", "nested/no-extension",
    };
    for (const std::string& name : transferOnlyNames) {
        require(!cardputer::isWorkspaceTextFile(name),
                "Transfer-only workspace file accepted as text: " + name);
        require(!cardputer::requestsWorkspaceAccess("inspect " + name),
                "Transfer-only extension enabled workspace tools: " + name);
    }
    require(!cardputer::isValidWorkspaceFilename("draft.tmp"),
            "Reserved temporary workspace suffix accepted");
    require(!cardputer::isValidWorkspaceFilename("backup.bak"),
            "Reserved backup workspace suffix accepted");
    require(cardputer::isValidStorageRelativePath("Проекты/заметки/идея.md", 512),
            "Valid nested UTF-8 storage path rejected");
    require(cardputer::isValidStorageRelativePath("src/main.cpp", 512),
            "Valid source storage path rejected");
    require(!cardputer::isValidStorageRelativePath("../secret.txt", 512),
            "Parent traversal storage path accepted");
    require(!cardputer::isValidStorageRelativePath("notes//draft.md", 512),
            "Empty storage path segment accepted");
    require(!cardputer::isValidStorageRelativePath("C:/secret.txt", 512),
            "Drive-qualified storage path accepted");
    require(!cardputer::isValidStorageRelativePath("/absolute.txt", 512),
            "Absolute storage path accepted");
    require(!cardputer::isValidStorageRelativePath(std::string("bad\x01.txt", 8), 512),
            "Control character in storage path accepted");
    require(!cardputer::isValidStorageRelativePath("notes/./draft.md", 512),
            "Current-directory storage segment accepted");
    require(!cardputer::isValidStorageRelativePath("notes/../draft.md", 512),
            "Nested parent traversal storage path accepted");
}

void testContextWindowBudget()
{
    const std::vector<cardputer::Message> messages = {
        {"user", "first"},
        {"assistant", "second"},
        {"user", "third"},
    };
    const cardputer::ContextWindowResult all =
        cardputer::fitMessagesToByteBudget(messages, 4096);
    require(all.droppedMessages == 0 && all.retained.size() == 3,
            "Context budget dropped messages that fit");
    const cardputer::ContextWindowResult newest =
        cardputer::fitMessagesToByteBudget(messages, 22);
    require(newest.droppedMessages == 2 && newest.retained.size() == 1 &&
                newest.retained.front().content == "third",
            "Context budget did not retain the newest complete message");
    const cardputer::ContextWindowResult zero =
        cardputer::fitMessagesToByteBudget(messages, 0);
    require(zero.droppedMessages == 3 && zero.retained.empty(),
            "Zero context budget retained messages");
    const std::vector<cardputer::Message> oversized = {{"user", std::string(128, 'x')}};
    const cardputer::ContextWindowResult one =
        cardputer::fitMessagesToByteBudget(oversized, 16);
    require(one.droppedMessages == 0 && one.retained.size() == 1,
            "Context budget discarded the only newest message");
    std::vector<cardputer::Message> owned = {
        {"user", "first"},
        {"assistant", "second"},
        {"user", std::string(16384, 'x')},
    };
    const char* promptStorage = owned.back().content.data();
    const cardputer::ContextWindowResult moved =
        cardputer::fitOwnedMessagesToByteBudget(std::move(owned), 16404);
    require(moved.droppedMessages == 2 && moved.retained.size() == 1 &&
                moved.retained.front().content.data() == promptStorage,
            "Owned context fitting copied the retained large prompt");
    std::vector<cardputer::Message> finalMessages = moved.retained;
    finalMessages.push_back({"assistant", std::string(16384, 'y')});
    const std::vector<cardputer::Message> dropped =
        cardputer::takeMessagesDroppedToByteBudget(std::move(finalMessages), 16409);
    require(dropped.size() == 1 && dropped.front().content.size() == 16384,
            "Owned context fitting did not return the omitted prefix");
}

void testJsonStringReader()
{
    const std::string exactContent(16384, 'x');
    const std::string exactRecord = "{\"sequence\":1,\"content\":\"" +
        exactContent + "\",\"role\":\"user\"}";
    MemoryJsonReader exactReader(exactRecord);
    const cardputer::json_reader::JsonStringValueResult exact =
        cardputer::json_reader::readObjectStringField(
            exactReader, "content", 64, 16384);
    require(exact.success && exact.value == exactContent,
            "Exact JSON string boundary decoding failed");

    const std::string escapedRecord =
        "{\"content\":\"quote: \\\" slash: \\\\ unicode: \\u041f"
        " emoji: \\uD83D\\uDE00\"}";
    MemoryJsonReader escapedReader(escapedRecord);
    const cardputer::json_reader::JsonStringValueResult escaped =
        cardputer::json_reader::readObjectStringField(
            escapedReader, "content", 64, 16384);
    require(escaped.success && escaped.value == "quote: \" slash: \\ unicode: П emoji: 😀",
            "Escaped JSON string decoding failed");

    const std::string duplicateRecord =
        "{\"content\":\"first\",\"content\":\"second\"}";
    MemoryJsonReader duplicateReader(duplicateRecord);
    require(!cardputer::json_reader::readObjectStringField(
                 duplicateReader, "content", 64, 16384).success,
            "Duplicate JSON string field was accepted");

    MemoryJsonReader exactMeasureReader(exactRecord);
    const cardputer::json_reader::JsonStringLengthResult exactMeasure =
        cardputer::json_reader::measureObjectStringField(
            exactMeasureReader, "content", 64, 16384);
    require(exactMeasure.success && exactMeasure.bytes == 16384,
            "Exact JSON string boundary measurement failed");

    const std::string unicodeRecord =
        "{\"ignored\":{\"nested\":true},\"c\\u006Fntent\":\"Привет "
        "\\u041C\\u0438\\u0440 \\uD83D\\uDE00\"}";
    const std::string decodedUnicode = "Привет Мир 😀";
    MemoryJsonReader unicodeReader(unicodeRecord);
    const cardputer::json_reader::JsonStringLengthResult unicode =
        cardputer::json_reader::measureObjectStringField(
            unicodeReader, "content", 64, 16384);
    require(unicode.success && unicode.bytes == decodedUnicode.size(),
            "Raw and escaped Unicode JSON measurement failed");

    const std::string maximumContent(131072, 'm');
    const std::string maximumRecord = "{\"content\":\"" + maximumContent + "\"}";
    MemoryJsonReader maximumReader(maximumRecord);
    const cardputer::json_reader::JsonStringLengthResult maximum =
        cardputer::json_reader::measureObjectStringField(
            maximumReader, "content", 64, 131072);
    require(maximum.success && maximum.bytes == 131072,
            "Maximum JSON string measurement failed");

    const std::string emptyRecord = "{\"content\":\"\"}";
    MemoryJsonReader emptyReader(emptyRecord);
    const cardputer::json_reader::JsonStringLengthResult empty =
        cardputer::json_reader::measureObjectStringField(
            emptyReader, "content", 64, 16384);
    require(empty.success && empty.bytes == 0,
            "Empty JSON string measurement did not remain caller-controlled");

    const std::string oversizedRecord =
        "{\"content\":\"" + std::string(131073, 'o') + "\"}";
    MemoryJsonReader oversizedReader(oversizedRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 oversizedReader, "content", 64, 131072).success,
            "JSON string above the measurement limit was accepted");

    const std::string malformedEscapeRecord = "{\"content\":\"bad\\q\"}";
    MemoryJsonReader malformedEscapeReader(malformedEscapeRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 malformedEscapeReader, "content", 64, 16384).success,
            "Malformed JSON escape was accepted during measurement");

    const std::string malformedSurrogateRecord =
        "{\"content\":\"bad \\uD83Dvalue\"}";
    MemoryJsonReader malformedSurrogateReader(malformedSurrogateRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 malformedSurrogateReader, "content", 64, 16384).success,
            "Malformed JSON surrogate pair was accepted during measurement");

    std::string invalidUtf8Record = "{\"content\":\"bad ";
    invalidUtf8Record.push_back(static_cast<char>(0xF0));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record.push_back(static_cast<char>(0x80));
    invalidUtf8Record += "\"}";
    MemoryJsonReader invalidUtf8Reader(invalidUtf8Record);
    require(!cardputer::json_reader::measureObjectStringField(
                 invalidUtf8Reader, "content", 64, 16384).success,
            "Invalid literal UTF-8 was accepted during JSON measurement");

    MemoryJsonReader measuredDuplicateReader(duplicateRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 measuredDuplicateReader, "content", 64, 16384).success,
            "Duplicate JSON string field was accepted during measurement");

    const std::string missingRecord = "{\"role\":\"user\"}";
    MemoryJsonReader missingReader(missingRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 missingReader, "content", 64, 16384).success,
            "Missing JSON string field was accepted during measurement");

    const std::string trailingRecord = "{\"content\":\"ok\"}garbage";
    MemoryJsonReader trailingReader(trailingRecord);
    require(!cardputer::json_reader::measureObjectStringField(
                 trailingReader, "content", 64, 16384).success,
            "Trailing JSON data was accepted during measurement");
}

void testLargePromptTitle()
{
    const std::string prompt = std::string(16384, 'x');
    const std::string title = cardputer::makeChatTitle(prompt, 36);
    require(title == std::string(33, 'x') + "...",
            "Large prompt title was not bounded to its display-cell budget");
    std::string unicodePrompt;
    unicodePrompt.reserve(16384);
    for (std::size_t index = 0; index < 5461; ++index) {
        unicodePrompt += "\xE7\x94\xA8";
    }
    unicodePrompt += 'x';
    const std::string unicodeTitle = cardputer::makeChatTitle(unicodePrompt, 36);
    require(unicodePrompt.size() == 16384 && unicodeTitle.size() <= 51 &&
                unicodeTitle.size() >= 3 &&
                unicodeTitle.compare(unicodeTitle.size() - 3, 3, "...") == 0,
            "Large Unicode prompt title was not bounded without full-prompt growth");
}

void testInstructionPrecedence()
{
    const std::string scoped = cardputer::buildScopedInstructions(
        "project", "chat", "request", "summary");
    const std::string resolved = cardputer::buildUserInstructionScopes("global", scoped);
    const std::size_t global = resolved.find("Global instructions");
    const std::size_t project = resolved.find("Project instructions");
    const std::size_t chat = resolved.find("Chat-specific instructions");
    const std::size_t request = resolved.find("Instructions for this request");
    const std::size_t summary = resolved.find("Conversation summary");
    require(global != std::string::npos && global < project && project < chat &&
                chat < summary && summary < request,
            "Scoped instruction precedence is not global -> project -> chat -> request");
    require(cardputer::buildScopedInstructions("", "chat", "", "").find(
                "Project instructions supplied by the user") == std::string::npos,
            "Empty instruction scope produced a synthetic section");
    require(cardputer::kMaximumRequestInstructionsBytes == 2048,
            "One-request instruction limit changed unexpectedly");
}

void testRequestOutputBudget()
{
    require(cardputer::resolveRequestOutputTokens(2048, 0) == 2048,
            "Missing request output override did not preserve the project budget");
    require(cardputer::resolveRequestOutputTokens(2048, 8192) == 8192,
            "Request output override did not take precedence over the project budget");
}

void testProjectRequestPolicy()
{
    cardputer::Settings settings = {};
    settings.model = "global-model";
    cardputer::ProjectDocument project = {};
    project.contextByteBudget = 65536;
    project.maximumOutputTokens = 2048;
    project.automaticCompaction = true;
    cardputer::ChatDocument chat = {};
    const cardputer::ResolvedProjectRequestPolicy inherited =
        cardputer::resolveProjectRequestPolicy(settings, project, chat, 0);
    require(inherited.model == "global-model" &&
                inherited.contextByteBudget == 65536 &&
                inherited.maximumOutputTokens == 2048 &&
                inherited.automaticCompaction,
            "Project request policy did not inherit global/project defaults");
    require(!cardputer::shouldAutomaticallyCompactRequest(inherited, 0) &&
                cardputer::shouldAutomaticallyCompactRequest(inherited, 1),
            "Automatic compaction gate ignored the dropped-message boundary");

    project.model = "project-model";
    project.automaticCompaction = false;
    chat.model = "chat-model";
    const String settingsModel = settings.model;
    const String projectModel = project.model;
    const String chatModel = chat.model;
    const std::uint32_t contextByteBudget = project.contextByteBudget;
    const std::uint32_t maximumOutputTokens = project.maximumOutputTokens;
    const bool automaticCompaction = project.automaticCompaction;
    const cardputer::ResolvedProjectRequestPolicy overridden =
        cardputer::resolveProjectRequestPolicy(settings, project, chat, 8192);
    require(overridden.model == "chat-model" &&
                overridden.maximumOutputTokens == 8192 &&
                !overridden.automaticCompaction &&
                !cardputer::shouldAutomaticallyCompactRequest(overridden, 4),
            "Chat/request overrides did not resolve deterministically");
    require(settings.model == settingsModel && project.model == projectModel &&
                chat.model == chatModel &&
                project.contextByteBudget == contextByteBudget &&
                project.maximumOutputTokens == maximumOutputTokens &&
                project.automaticCompaction == automaticCompaction,
            "Request-policy resolution modified one of its input scopes");

    chat.model.clear();
    const cardputer::ResolvedProjectRequestPolicy projectOverride =
        cardputer::resolveProjectRequestPolicy(settings, project, chat, 0);
    require(projectOverride.model == "project-model",
            "Empty chat model did not inherit the project model");
}

void testContextUsage()
{
    cardputer::ChatDocument chat = {};
    chat.summary.messageCount = 10;
    chat.summarizedMessageCount = 2;
    chat.messages = {
        {"user", "a"},
        {"assistant", "bb"},
        {"user", "ccc"},
        {"assistant", "dddd"},
    };
    const cardputer::ContextUsage bounded = cardputer::resolveContextUsage(chat, 29);
    require(bounded.retainedBytes == 29 && bounded.retainedMessages == 1 &&
                bounded.droppedMessages == 7 && bounded.summarizedMessages == 2 &&
                bounded.totalMessages == 10,
            "Context usage did not include the bounded-tail and role overhead");
    const cardputer::ContextUsage oversizedNewest =
        cardputer::resolveContextUsage(chat, 1);
    require(oversizedNewest.retainedBytes == 29 &&
                oversizedNewest.retainedMessages == 1 &&
                oversizedNewest.droppedMessages == 7,
            "Context usage did not preserve the newest oversized message");
    const cardputer::ContextUsage disabled = cardputer::resolveContextUsage(chat, 0);
    require(disabled.retainedBytes == 0 && disabled.retainedMessages == 0 &&
                disabled.droppedMessages == 8,
            "Zero context budget retained unsummarized messages");
}

void testRetryRequestPreparation()
{
    const std::vector<cardputer::Message> failedHistory = {
        {"user", "first"},
        {"assistant", "answer"},
        {"user", "retry exactly once"},
    };
    const cardputer::RetryRequestResult retry = cardputer::prepareRetryRequest(
        failedHistory, 4096);
    require(retry.success && retry.prompt == "retry exactly once",
            "Retry preparation did not retain the failed user request");
    require(retry.messages.size() == failedHistory.size(),
            "Retry preparation duplicated or removed a stored message");
    require(failedHistory.size() == 3 && failedHistory.back().content == "retry exactly once",
            "Retry preparation modified its input history");
    require(!cardputer::prepareRetryRequest(
                {{"user", "done"}, {"assistant", "answer"}}, 4096).success,
            "Retry preparation accepted a completed assistant turn");
}

void testSummarizedChatTail()
{
    cardputer::ChatDocument chat = {};
    chat.summary.messageCount = 12;
    chat.summarizedMessageCount = 8;
    chat.messages = {
        {"user", "6"},
        {"assistant", "7"},
        {"user", "8"},
        {"assistant", "9"},
        {"user", "10"},
        {"assistant", "11"},
    };
    const std::vector<cardputer::Message> active =
        cardputer::unsummarizedChatTail(chat);
    require(active.size() == 4 && active.front().content == "8" &&
                active.back().content == "11",
            "Summarized messages remained in the active context tail");
    require(chat.messages.size() == 6,
            "Active-tail calculation modified raw chat messages");
    const std::vector<cardputer::Message> owned =
        cardputer::takeUnsummarizedChatTail(std::move(chat));
    require(owned.size() == 4 && owned.front().content == "8" &&
                owned.back().content == "11",
            "Owned active-tail calculation changed the summarized boundary");
}

void testContextSummaryPrompt()
{
    const std::vector<cardputer::Message> messages = {
        {"user", "first"},
        {"assistant", "second"},
    };
    const cardputer::ContextSummaryPromptResult result =
        cardputer::buildContextSummaryPrompt("earlier", messages, 1024);
    require(result.success, "Context summary prompt should fit");
    require(result.includedMessages == 2,
            "Context summary prompt should include both messages");
    require(result.prompt.find("Previous summary:\nearlier") != std::string::npos,
            "Context summary prompt should preserve the previous summary");
    require(result.prompt.find("You: first\nAI: second\n") != std::string::npos,
            "Context summary prompt should preserve roles and content");
    const cardputer::ContextSummaryPromptResult tooSmall =
        cardputer::buildContextSummaryPrompt("earlier", messages, 32);
    require(!tooSmall.success,
            "Context summary prompt should reject an insufficient allocation budget");
}

void testWorkspaceRouting()
{
    require(!cardputer::requestsWorkspaceAccess("Когда появится Cardputer Zero?"),
            "Ordinary Russian question enabled workspace tools");
    require(!cardputer::requestsWorkspaceAccess("Read the question and answer briefly"),
            "Ordinary English instruction enabled workspace tools");
    require(cardputer::requestsWorkspaceAccess("Сохрани ответ в файл release.md"),
            "Explicit Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess(
                "Можешь набросапть простой тестовый скрипт на Python, и сохранить его?"),
            "Natural Russian Python save request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("ПОКАЖИ, КАКИЕ ФАЙЛЫ ЕСТЬ НА SD"),
            "Uppercase Russian file request did not enable workspace tools");
    require(cardputer::requestsWorkspaceAccess("/file create notes.txt"),
            "Explicit file command did not enable workspace tools");
    require(cardputer::requestsWorkspaceWrite(
                "Можешь набросапть простой тестовый скрипт на Python, и сохранить его?"),
            "Natural Russian Python save request did not require a file write");
    require(cardputer::requestsWorkspaceWrite("Create a downloadable file notes.md"),
            "English create request did not require a file write");
    require(!cardputer::requestsWorkspaceWrite("ПОКАЖИ, КАКИЕ ФАЙЛЫ ЕСТЬ НА SD"),
            "Read-only file request incorrectly required a file write");
    std::string inertBoundaryPrompt;
    inertBoundaryPrompt.reserve(16384);
    for (std::size_t index = 0; index < 5461; ++index) {
        inertBoundaryPrompt += "用";
    }
    inertBoundaryPrompt += 'x';
    require(inertBoundaryPrompt.size() == 16384,
            "Workspace routing fixture is not exactly 16384 bytes");
    require(!cardputer::requestsWorkspaceAccess(inertBoundaryPrompt) &&
                !cardputer::requestsWorkspaceWrite(inertBoundaryPrompt) &&
                !cardputer::requestsWebSearch(inertBoundaryPrompt),
            "Inert exact-boundary prompt enabled a tool policy");
    const std::string trailingCommand = " СОХРАНИ В ФАЙЛ";
    std::string commandBoundaryPrompt(16384 - trailingCommand.size(), 'x');
    commandBoundaryPrompt += trailingCommand;
    require(cardputer::requestsWorkspaceAccess(commandBoundaryPrompt) &&
                cardputer::requestsWorkspaceWrite(commandBoundaryPrompt),
            "Exact-boundary trailing Russian file command was not scanned");
}

void testWebSearchRouting()
{
    require(cardputer::requestsWebSearch("Когда появится Cardputer Zero?"),
            "Current Russian question did not enable web search");
    require(cardputer::requestsWebSearch("Find the latest Cardputer firmware news"),
            "Current English question did not enable web search");
    require(cardputer::requestsWebSearch("/search Cardputer ADV"),
            "Explicit search command did not enable web search");
    require(!cardputer::requestsWebSearch("Напиши краткое эссе о кошках"),
            "Ordinary Russian request enabled web search");
    require(!cardputer::requestsWebSearch("What is two plus two?"),
            "Stable English question enabled web search");
    require(cardputer::isWebSearchToolName("web_search"),
            "Canonical web search tool name was rejected");
    require(cardputer::isWebSearchToolName("WebSearch"),
            "Proxy web search tool alias was rejected");
    require(cardputer::isWebSearchToolName("web-search"),
            "Hyphenated web search tool alias was rejected");
    require(!cardputer::isWebSearchToolName("write_file"),
            "File tool name was classified as web search");
    require(cardputer::isWebFetchToolName("WebFetch"),
            "Proxy web fetch tool alias was rejected");
    require(cardputer::isWebFetchToolName("web_fetch"),
            "Canonical web fetch tool name was rejected");
    require(!cardputer::isWebFetchToolName("read_file"),
            "File tool name was classified as web fetch");
}

void testSshTerminalFiltering()
{
    cardputer::SshTerminalText state = {"prompt", "", false};
    const std::string colored = "\x1B[31m red\x1B[0m\nnext\rreplace";
    state = cardputer::appendSshTerminalBytes(
        state, reinterpret_cast<const std::uint8_t*>(colored.data()),
        colored.size(), 1024);
    require(state.text == "prompt red\nreplace", "SSH ANSI or carriage-return filtering failed");
    const std::string clear = "\x1B[2Jclean";
    state = cardputer::appendSshTerminalBytes(
        state, reinterpret_cast<const std::uint8_t*>(clear.data()),
        clear.size(), 1024);
    require(state.text == "clean", "SSH clear-screen filtering failed");
    const auto lines = cardputer::sshTerminalVisibleLines(state, 10, 3);
    require(lines.size() == 3 && lines.back() == "clean",
            "SSH terminal viewport failed");
    const std::string history = "one\ntwo\nthree\nfour\nfive";
    state = {history, "", false};
    const auto scrolled = cardputer::sshTerminalLinesFromBottom(state, 10, 2, 2);
    require(scrolled.size() == 2 && scrolled[0] == "two" && scrolled[1] == "three",
            "SSH terminal scrollback offset failed");
    const auto bounded = cardputer::sshTerminalLinesFromBottom(state, 10, 2, 99);
    require(bounded.size() == 2 && bounded[0] == "one" && bounded[1] == "two",
            "SSH terminal scrollback bound failed");
}

void testDocumentReader()
{
    using cardputer::DocumentReaderMode;
    require(cardputer::detectDocumentReaderMode("notes.MD") == DocumentReaderMode::Markdown,
            "Markdown reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("table.csv") == DocumentReaderMode::Csv,
            "CSV reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("data.json") == DocumentReaderMode::Json,
            "JSON reader mode detection failed");
    require(cardputer::detectDocumentReaderMode("page.HTML") == DocumentReaderMode::HtmlSource,
            "HTML source reader mode detection failed");
    require(cardputer::formatDocumentChunk(DocumentReaderMode::Markdown,
                                            "# Title\n- item") ==
                "Title\n• item",
            "Markdown reader formatting failed");
    require(cardputer::formatDocumentChunk(DocumentReaderMode::Csv,
                                            "name,\"one,two\"") ==
                "name | one,two",
            "Quoted CSV reader formatting failed");
    const std::string json = cardputer::formatDocumentChunk(
        DocumentReaderMode::Json, "{\"ok\":true,\"n\":2}");
    require(json.find("\n") != std::string::npos &&
                json.find("\"ok\": true") != std::string::npos,
            "JSON reader formatting failed");
    require(cardputer::documentSpeechText(DocumentReaderMode::HtmlSource,
                                           "<p>Hello &amp; bye</p>") ==
                "Hello & bye ",
            "HTML speech extraction failed");
}

void testOfflineCalculator()
{
    const cardputer::CalculationResult precedence = cardputer::calculateExpression("2+3*4");
    require(precedence.success && precedence.value == 14.0,
            "Calculator precedence failed");
    const cardputer::CalculationResult parentheses = cardputer::calculateExpression("(2+3)*4");
    require(parentheses.success && parentheses.value == 20.0,
            "Calculator parentheses failed");
    const cardputer::CalculationResult unary = cardputer::calculateExpression("-2.5 + 1");
    require(unary.success && unary.value == -1.5, "Calculator unary operator failed");
    require(!cardputer::calculateExpression("1/0").success,
            "Calculator accepted division by zero");
    require(!cardputer::calculateExpression("2+").success,
            "Calculator accepted an incomplete expression");
    require(cardputer::formatCalculationResult(1.25) == "1.25",
            "Calculator result formatting failed");
}

}  // namespace

int main()
{
    try {
        testUtf8Backspace();
        testRussianLayout();
        testWrapping();
        testSse();
        testUtf8Validation();
        testJsonStringReader();
        testWavHeader();
        testChatText();
        testContextWindowBudget();
        testLargePromptTitle();
        testInstructionPrecedence();
        testToolPolicyContracts();
        testToolMessageIntent();
        testToolMessageIntentCodec();
        testToolCatalogAndRequestPlan();
        testPendingToolPreview();
        testToolPolicyPrecedence();
        testToolPolicyRoadmapExamples();
        testToolPolicyIntentAvailabilityAndErrors();
        testToolPolicyCodecDefaultsAndLegacyMigration();
        testToolPolicyCodecRoundTrip();
        testToolPolicyCodecRejectsMalformedText();
        testRequestOutputBudget();
        testProjectRequestPolicy();
        testContextUsage();
        testRetryRequestPreparation();
        testSummarizedChatTail();
        testContextSummaryPrompt();
        testWorkspaceRouting();
        testWebSearchRouting();
        testSshTerminalFiltering();
        testDocumentReader();
        testOfflineCalculator();
        std::cout << "host_tests: PASS\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "host_tests: FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
