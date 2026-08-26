#pragma once

#include <WebServer.h>

#include <array>
#include <cstddef>

namespace cardputer {

enum class WebConsoleRouteHandler : std::size_t {
    Root,
    Login,
    Logout,
    Session,
    CloseConsole,
    State,
    StorageConfirm,
    SelectProject,
    NewProject,
    ProjectSettings,
    ProjectSettingsRawComplete,
    ProjectSettingsRawData,
    RenameProject,
    DuplicateProject,
    ArchiveProject,
    DeleteProject,
    ProjectLinks,
    ProjectLinkUpdate,
    Prompt,
    PromptRawComplete,
    PromptRawData,
    PromptRetry,
    SelectChat,
    NewChat,
    Instructions,
    InstructionsRawComplete,
    InstructionsRawData,
    ChatCompact,
    ChatPermissions,
    RenameChat,
    PinChat,
    ArchiveChat,
    DuplicateChat,
    ExportChat,
    ExportChatBundle,
    ImportChatBundle,
    DeleteChat,
    ClearChat,
    ArchivedMessages,
    Settings,
    Models,
    Diagnostics,
    DiagnosticMetrics,
    PythonStart,
    SshSettings,
    SshSelect,
    SshDelete,
    SshStart,
    SshTrust,
    SshInput,
    SshResize,
    SshOutput,
    SshStop,
    SftpList,
    SftpDownload,
    SftpUpload,
    SshKeyComplete,
    SshKeyUpload,
    QrShow,
    QrFile,
    QrClose,
    FileRead,
    FileSave,
    FileRename,
    FileDelete,
    FileDownload,
    FileUploadComplete,
    FileUploadData,
    NotFound,
    Count,
};

using WebConsoleHandler = void (*)();
using WebConsoleRouteGuard = bool (*)(WebConsoleRouteHandler route);
constexpr std::size_t kWebConsoleRouteHandlerCount =
    static_cast<std::size_t>(WebConsoleRouteHandler::Count);

struct WebConsoleRouteHandlers {
    std::array<WebConsoleHandler, kWebConsoleRouteHandlerCount> items;
    WebConsoleRouteGuard guard;
};

void configureWebConsoleRoutes(WebServer& server,
                               const WebConsoleRouteHandlers& handlers);

}  // namespace cardputer
