import { readFileSync } from "node:fs";

const sourcePath = new URL("../firmware/CardputerAssistant/src/web_console.cpp", import.meta.url);
const source = readFileSync(sourcePath, "utf8");
const functionStart = source.indexOf("void sendConsolePage()\n{");
const functionEnd = source.indexOf("\nvoid sendLoginPage()", functionStart);

if (functionStart < 0 || functionEnd < 0) {
    throw new Error("Could not locate sendConsolePage in web_console.cpp");
}

const functionSource = source.slice(functionStart, functionEnd);
const literals = [...functionSource.matchAll(/R"HTML\(([\s\S]*?)\)HTML"/g)];
if (literals.length !== 2) {
    throw new Error(`Expected two console HTML literals, found ${literals.length}`);
}

const page = `${literals[0][1]}const csrf='host-test';\n${literals[1][1]}`;
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
