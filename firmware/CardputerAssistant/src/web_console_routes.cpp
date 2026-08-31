#include "web_console_routes.h"

#include "web_console_metrics.h"

#include <functional>

namespace cardputer {
namespace {

WebServer* routeServer = nullptr;

std::function<void()> handler(const WebConsoleRouteHandlers& handlers,
                              WebConsoleRouteHandler route)
{
    const WebConsoleHandler selected =
        handlers.items[static_cast<std::size_t>(route)];
    const WebConsoleRouteGuard guard = handlers.guard;
    return [selected, guard, route]() {
        const String endpoint = routeServer->uri();
        beginWebRequestMetrics(endpoint.c_str());
        if (guard == nullptr || guard(route)) {
            selected();
        }
        finishWebRequestMetrics();
    };
}

std::function<void()> rawHandler(const WebConsoleRouteHandlers& handlers,
                                 WebConsoleRouteHandler route)
{
    const WebConsoleHandler selected =
        handlers.items[static_cast<std::size_t>(route)];
    return [selected]() { selected(); };
}

}  // namespace

void configureWebConsoleRoutes(WebServer& server,
                               const WebConsoleRouteHandlers& handlers)
{
    routeServer = &server;
    const char* headers[] = {
        "Cookie",
        "X-CardMind-CSRF",
        "Content-Type",
        "Content-Length",
        "Transfer-Encoding",
        "X-CardMind-Model-Encoded",
        "X-CardMind-Context-Bytes",
        "X-CardMind-Output-Tokens",
        "X-CardMind-Auto-Compact",
        "X-CardMind-Tool-Intent",
        "X-CardMind-Tool-Policy",
        "X-CardMind-Ssh-Profile-Encoded",
    };
    server.collectHeaders(headers, 12);
    server.on("/", HTTP_GET, handler(handlers, WebConsoleRouteHandler::Root));
    server.on("/login", HTTP_POST, handler(handlers, WebConsoleRouteHandler::Login));
    server.on("/logout", HTTP_POST, handler(handlers, WebConsoleRouteHandler::Logout));
    server.on("/api/session", HTTP_GET, handler(handlers, WebConsoleRouteHandler::Session));
    server.on("/api/console/close", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::CloseConsole));
    server.on("/api/state", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/pending", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::Pending));
    server.on("/api/status", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/activity", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/storage/confirm", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::StorageConfirm));
    server.on("/api/projects", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/project/select", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SelectProject));
    server.on("/api/project/new", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::NewProject));
    server.on("/api/project/settings", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ProjectSettings),
              rawHandler(handlers, WebConsoleRouteHandler::ProjectSettingsRawData));
    server.on("/api/project/settings/raw", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ProjectSettingsRawComplete),
              rawHandler(handlers, WebConsoleRouteHandler::ProjectSettingsRawData));
    server.on("/api/project/rename", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::RenameProject));
    server.on("/api/project/duplicate", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::DuplicateProject));
    server.on("/api/project/archive", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ArchiveProject));
    server.on("/api/project/delete", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::DeleteProject));
    server.on("/api/project/links", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::ProjectLinks));
    server.on("/api/project/link", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ProjectLinkUpdate));
    server.on("/api/chats", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/chat", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/files", HTTP_GET, handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/ssh/state", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/prompt", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::Prompt),
              rawHandler(handlers, WebConsoleRouteHandler::PromptRawData));
    server.on("/api/prompt/raw", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PromptRawComplete),
              rawHandler(handlers, WebConsoleRouteHandler::PromptRawData));
    server.on("/api/prompt/retry", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PromptRetry));
    server.on("/api/pending/allow-once", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PendingAllowOnce));
    server.on("/api/pending/allow-chat", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PendingAllowChat));
    server.on("/api/pending/deny", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PendingDeny));
    server.on("/api/pending/acknowledge", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PendingAcknowledge));
    server.on("/api/chat/select", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SelectChat));
    server.on("/api/chat/new", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::NewChat));
    server.on("/api/chat/instructions", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::Instructions),
              rawHandler(handlers, WebConsoleRouteHandler::InstructionsRawData));
    server.on("/api/chat/instructions/raw", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::InstructionsRawComplete),
              rawHandler(handlers, WebConsoleRouteHandler::InstructionsRawData));
    server.on("/api/chat/settings", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ChatSettings));
    server.on("/api/chat/compact", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ChatCompact));
    server.on("/api/chat/permissions", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ChatPermissions));
    server.on("/api/chat/rename", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::RenameChat));
    server.on("/api/chat/pin", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PinChat));
    server.on("/api/chat/archive", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ArchiveChat));
    server.on("/api/chat/duplicate", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::DuplicateChat));
    server.on("/api/chat/export", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ExportChat));
    server.on("/api/chat/export-bundle", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ExportChatBundle));
    server.on("/api/chat/import", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ImportChatBundle));
    server.on("/api/chat/delete", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::DeleteChat));
    server.on("/api/chat/clear", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::ClearChat));
    server.on("/api/chat/archived", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::ArchivedMessages));
    server.on("/api/settings", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::Settings));
    server.on("/api/settings", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::State));
    server.on("/api/models", HTTP_GET, handler(handlers, WebConsoleRouteHandler::Models));
    server.on("/api/diagnostics", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::Diagnostics));
    server.on("/api/diagnostics/metrics", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::DiagnosticMetrics));
    server.on("/api/python/start", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::PythonStart));
    server.on("/api/ssh/settings", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshSettings));
    server.on("/api/ssh/select", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshSelect));
    server.on("/api/ssh/delete", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshDelete));
    server.on("/api/ssh/start", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshStart));
    server.on("/api/ssh/trust", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshTrust));
    server.on("/api/ssh/input", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshInput));
    server.on("/api/ssh/resize", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshResize));
    server.on("/api/ssh/output", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::SshOutput));
    server.on("/api/ssh/stop", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshStop));
    server.on("/api/ssh/sftp/list", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::SftpList));
    server.on("/api/ssh/sftp/download", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SftpDownload));
    server.on("/api/ssh/sftp/upload", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SftpUpload));
    server.on("/api/ssh/key", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::SshKeyComplete),
              handler(handlers, WebConsoleRouteHandler::SshKeyUpload));
    server.on("/api/qr/show", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::QrShow));
    server.on("/api/qr/file", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::QrFile));
    server.on("/api/qr/close", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::QrClose));
    server.on("/api/file", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::FileRead));
    server.on("/api/file/save", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::FileSave));
    server.on("/api/file/rename", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::FileRename));
    server.on("/api/file/delete", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::FileDelete));
    server.on("/api/file/download", HTTP_GET,
              handler(handlers, WebConsoleRouteHandler::FileDownload));
    server.on("/api/file/upload", HTTP_POST,
              handler(handlers, WebConsoleRouteHandler::FileUploadComplete),
              handler(handlers, WebConsoleRouteHandler::FileUploadData));
    server.onNotFound(handler(handlers, WebConsoleRouteHandler::NotFound));
}

}  // namespace cardputer
