import { readFileSync } from "node:fs";
import { gunzipSync } from "node:zlib";

const pagePath = new URL(
    "../firmware/CardputerAssistant/assets/web_console.html",
    import.meta.url,
);
const headerPath = new URL(
    "../firmware/CardputerAssistant/src/web_console_asset.h",
    import.meta.url,
);
const page = readFileSync(pagePath, "utf8");
const header = readFileSync(headerPath, "utf8");
const compressed = Buffer.from(
    [...header.matchAll(/0x([0-9a-f]{2})/g)]
        .map((match) => Number.parseInt(match[1], 16)),
);
const embeddedPage = gunzipSync(compressed).toString("utf8");
if (embeddedPage !== page) {
    throw new Error("Generated Web Console asset is stale");
}
const requiredFragments = [
    'data-view="chat"',
    'data-view="files"',
    'data-view="ssh"',
    'data-view="settings"',
    'id="messages"',
    'id="fileContent"',
    'id="workspaceFiles"',
    'id="sshTerminal"',
    'id="diagnostics"',
    'id="actionDialog"',
    'id="chatDetails"',
    'id="openChatDetails"',
    'id="fileEditorTitle"',
    'id="qrContent"',
    'id="showQr"',
    'id="qrFromFile"',
    'id="sshToolsEnabled"',
    'id="savePermissions"',
    'id="wifiSsid"',
    'id="refreshModels"',
    'id="startPython"',
    "fetch('/api/session'",
    "let csrf=''",
    'const changedSecrets=new Set()',
    'data-1p-ignore',
    'id="exportDiagnostics"',
    'id="contextMeter"',
    'id="loadOlder"',
    'id="clearChat"',
    'id="endConsole"',
    'class="mobile-nav"',
    'data-panel="chat"><span class="nav-glyph">01</span>Chat log</button>',
    'data-panel="files"><span class="nav-glyph">02</span>Workspace</button>',
    'data-panel="ssh"><span class="nav-glyph">03</span>Terminal</button>',
    'data-panel="settings"><span class="nav-glyph">04</span>Settings</button>',
    '[data-view="files"] .split-layout{height:auto;min-height:0}',
    '[data-view="files"] .file-editor{min-height:44vh}',
    '/api/file/upload?replace=1&name=',
    '/api/qr/show',
    '/api/qr/file?name=',
    '/api/chat/permissions',
    '/api/chat/archived',
    '/api/chat/clear',
    '/api/console/close',
    "x.address+'handoff?token='",
    'handoff_token',
];

for (const fragment of requiredFragments) {
    if (!page.includes(fragment)) {
        throw new Error(`Web console page is missing ${fragment}`);
    }
}

const forbiddenFragments = [
    "Chunk editor",
    "Save chunk",
    'id="previousFilePage"',
    'id="nextFilePage"',
    'maxlength="12288"',
    'data-panel="files"><span class="nav-glyph">02</span>Files</button>',
    "Open in Safari",
    "Open the same device address and sign in again",
];

for (const fragment of forbiddenFragments) {
    if (page.includes(fragment)) {
        throw new Error(`Web console page exposes internal file paging: ${fragment}`);
    }
}

const scriptMatch = page.match(/<script>([\s\S]*)<\/script>/);
if (scriptMatch === null) {
    throw new Error("Web console page does not contain a script block");
}

new Function(scriptMatch[1]);
console.log("WEB_CONSOLE_UI_TEST result=pass");
