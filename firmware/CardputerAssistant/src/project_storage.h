#pragma once

#include "app_types.h"

#include <cstddef>
#include <cstdint>

namespace cardputer {

constexpr std::uint32_t kProjectStorageFormatVersion = 2;
constexpr std::size_t kMaximumProjectTitleBytes = 120;
constexpr std::size_t kMaximumProjectInstructionsBytes = 16384;
constexpr std::size_t kMaximumProjectPageEntries = 32;

OperationResult initializeProjectStorage();
ProjectStorageManifestResult loadProjectStorageManifest();
OperationResult saveProjectStorageManifest(const ProjectStorageManifest& manifest);
OperationResult resetUncommittedProjectStorage();
ProjectDocumentResult createProject(const String& title);
ProjectDocumentResult loadProject(const String& id);
OperationResult saveProject(const ProjectDocument& project);
OperationResult deleteProject(const String& id);
OperationResult renameProject(const String& id, const String& title);
OperationResult setProjectArchived(const String& id, bool archived);
ProjectDocumentResult duplicateProject(const String& id, const String& title);
ProjectsPageResult listProjectsPage(std::uint32_t offset, std::size_t maximumEntries);

OperationResult upsertProjectChatSummary(const String& projectId,
                                         const ChatSummary& summary);
OperationResult removeProjectChatSummary(const String& projectId, const String& chatId);
ProjectChatsPageResult listProjectChatsPage(const String& projectId,
                                            std::uint32_t offset,
                                            std::size_t maximumEntries);

OperationResult linkSharedFileToProject(const String& projectId, const String& path);
OperationResult unlinkSharedFileFromProject(const String& projectId, const String& path);
SharedFileLinksPageResult listProjectSharedLinksPage(const String& projectId,
                                                     std::uint32_t offset,
                                                     std::size_t maximumEntries);
SharedFileLinkResult projectHasSharedFileLink(const String& projectId, const String& path);
SharedFileLinkResult sharedFileHasAnyProjectLink(const String& path);

String projectStorageRoot();
String projectDirectoryPath(const String& id);
String projectChatsDirectoryPath(const String& id);
String projectChatDirectoryPath(const String& projectId, const String& chatId);

}  // namespace cardputer
