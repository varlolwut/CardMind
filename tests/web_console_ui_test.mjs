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
const routesPath = new URL(
    "../firmware/CardputerAssistant/src/web_console_routes.cpp",
    import.meta.url,
);
const statePath = new URL(
    "../firmware/CardputerAssistant/src/web_console_state.cpp",
    import.meta.url,
);
const page = readFileSync(pagePath, "utf8").replace(/\r\n/g, "\n");
const header = readFileSync(headerPath, "utf8");
const routes = readFileSync(routesPath, "utf8");
const stateBuilder = readFileSync(statePath, "utf8");
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
    'role="textbox" aria-label="Interactive SSH terminal"',
    'id="sshKeyState"',
    'id="uploadSshKey" hidden',
    'class="row mobile-terminal-input"',
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
    'id="globalInstructions"',
    'id="diagnosticMetrics"',
    "fetch('/api/session'",
    "let csrf=''",
    'const changedSecrets=new Set()',
    'data-1p-ignore',
    'id="exportDiagnostics"',
    'id="contextMeter"',
    'id="loadOlder"',
    'id="clearChat"',
    'id="endConsole"',
    'id="projects"',
    'id="newProject"',
    "loadMoreProjectsButton.id='loadMoreProjects'",
    'id="projectInstructions"',
    'id="projectContextBudget"',
    'id="projectOutputTokens"',
    'id="projectAutoCompact"',
    "projectLinkButton.id='toggleProjectLink'",
    'id="previousFileWindow"',
    'id="nextFileWindow"',
    'Save window',
    'class="mobile-nav"',
    'data-panel="chat"><span class="nav-glyph">01</span>Chat log</button>',
    'data-panel="files"><span class="nav-glyph">02</span>Workspace</button>',
    'data-panel="ssh"><span class="nav-glyph">03</span>Terminal</button>',
    'data-panel="settings"><span class="nav-glyph">04</span>Settings</button>',
    '[data-view="files"] .split-layout{height:auto;min-height:0}',
    '[data-view="files"] .file-editor{min-height:44vh}',
    '.terminal-layout>*{min-width:0}',
    "request('/api/file/upload',{method:'POST',body:data})",
    '/api/projects',
    '/api/project/select',
    '/api/project/settings',
    '/api/project/links?offset=',
    '/api/project/link',
    '/api/qr/show',
    '/api/qr/file?name=',
    '/api/chat/permissions',
    '/api/chat/archived',
    '/api/chat/clear',
    '/api/console/close',
    "x.address+'handoff?token='",
    'handoff_token',
    'sshPollBusy',
    'output_base64',
    "event.type==='notice'",
    "userMessage.textContent='You: '+prompt",
    'pointer-events:none',
    "const stateEndpoints={status:'/api/status',projects:'/api/projects',chats:'/api/chats',chat:'/api/chat',files:'/api/files',ssh:'/api/ssh/state',settings:'/api/settings'}",
    'function monitorSshConnection()',
    'function shouldRenderRevision(kind,revision)',
    'requestAnimationFrame(()=>',
    'function chatDraftKey(id)',
    '/api/ssh/resize',
    "e.key==='Backspace')d='\\x7f'",
    "e.key==='ArrowUp')d='\\x1b[A'",
    'new ResizeObserver(queueTerminalResize)',
    'function terminalFeed(value)',
    'function terminalAlternateScreen(enabled)',
    'function terminalInsertCharacters(amount)',
    'function terminalDeleteCharacters(amount)',
    'function renderStatusState(s)',
    'function renderChatState(s)',
    'function renderFilesState(s)',
    'function renderSshState(s)',
    'function renderSettingsState(s)',
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

function readArrowFunction(name, nextMarker) {
    const marker = `const ${name}=`;
    const start = page.indexOf(marker);
    const end = page.indexOf(nextMarker, start + marker.length);
    if (start < 0 || end < 0) {
        throw new Error(`Cannot extract ${name} from the Web Console`);
    }
    return new Function(`return (${page.slice(start + marker.length, end)})`)();
}

const parseSseChunk = readArrowFunction(
    "parseSseChunk",
    ";const summarizeSseEvents=",
);
const promptStreamCompletionError = readArrowFunction(
    "promptStreamCompletionError",
    ";\nasync function sendPrompt",
);
const summarizeSseEvents = readArrowFunction(
    "summarizeSseEvents",
    ";const promptStreamCompletionError=",
);

const firstChunk = parseSseChunk(
    "",
    'data: {"type":"delta","delta":"Hel',
    false,
);
if (firstChunk.events.length !== 0 || firstChunk.buffer.length === 0) {
    throw new Error("A partial SSE event was committed before its frame ended");
}
const secondChunk = parseSseChunk(
    firstChunk.buffer,
    'lo"}\r\n\r\ndata: {"type":"done"}\r\n\r\n',
    false,
);
if (secondChunk.buffer !== "" || secondChunk.events.length !== 2 ||
    secondChunk.events[0].delta !== "Hello" ||
    secondChunk.events[1].type !== "done") {
    throw new Error("Split SSE events were not reassembled correctly");
}
const flushedFinalEvent = parseSseChunk(
    "",
    'data: {"type":"done"}',
    true,
);
if (flushedFinalEvent.buffer !== "" ||
    flushedFinalEvent.events.length !== 1 ||
    flushedFinalEvent.events[0].type !== "done") {
    throw new Error("A complete terminal event was lost at EOF");
}
if (promptStreamCompletionError("", "done", "") !== "") {
    throw new Error("A completed SSE stream was rejected");
}
if (!promptStreamCompletionError(firstChunk.buffer, "", "").includes("incomplete")) {
    throw new Error("A partial final SSE event was accepted");
}
if (!promptStreamCompletionError("", "", "").includes("before CardMind")) {
    throw new Error("EOF without a terminal SSE event was accepted");
}
if (promptStreamCompletionError("", "error", "API failed") !== "API failed") {
    throw new Error("An SSE error event did not preserve its message");
}
const summaryWithNotice = summarizeSseEvents([
    { type: "notice", error: "SSH terminal disconnected" },
    { type: "delta", delta: "OK" },
    { type: "done" },
], "", "");
if (summaryWithNotice.notice !== "SSH terminal disconnected" ||
    summaryWithNotice.delta !== "OK" || summaryWithNotice.terminalType !== "done") {
    throw new Error("An SSE notice was not preserved alongside streamed text");
}

const terminalStart = page.indexOf("const terminalScreen=");
const terminalEnd = page.indexOf("function appendTerminalRun", terminalStart);
if (terminalStart < 0 || terminalEnd < 0) {
    throw new Error("Cannot extract the SSH terminal parser");
}
const terminalHarness = new Function(
    "function renderTerminal(){}\n" +
    page.slice(terminalStart, terminalEnd) +
    ";return {screen:terminalScreen,feed:terminalFeed,reset:terminalReset};",
)();
terminalHarness.reset();
terminalHarness.feed("hello\bX\r\n\x1b[31mred\x1b[0m");
if (terminalHarness.screen.lines[0].map((cell) => cell.character).join("") !== "hellX") {
    throw new Error("SSH terminal backspace did not overwrite the previous cell");
}
if (terminalHarness.screen.lines[1].map((cell) => cell.character).join("") !== "red" ||
    terminalHarness.screen.lines[1][0].style !== "ansi-red") {
    throw new Error("SSH terminal ANSI color state was not applied");
}
terminalHarness.feed("\x1b[2J\x1b[Hready");
if (terminalHarness.screen.lines.length !== 1 ||
    terminalHarness.screen.lines[0].map((cell) => cell.character).join("") !== "ready") {
    throw new Error("SSH terminal clear-screen and cursor-home handling failed");
}
for (const endpoint of [
    "/api/status",
    "/api/projects",
    "/api/chats",
    "/api/chat",
    "/api/files",
    "/api/ssh/state",
    "/api/settings",
]) {
    if (!routes.includes(`server.on("${endpoint}"`)) {
        throw new Error(`Specialized Web Console route is missing: ${endpoint}`);
    }
}
if (!routes.includes("beginWebRequestMetrics(endpoint.c_str())") ||
    !routes.includes("finishWebRequestMetrics()")) {
    throw new Error("Web Console routes are not wrapped in request metrics");
}
if (stateBuilder.includes("listWorkspaceFiles()") ||
    stateBuilder.includes("loadSshProfiles(")) {
    throw new Error("Web Console state builders bypass the RAM indexes and read microSD");
}
terminalHarness.feed("\x1b[2Jabc\r\x1b[1@X\x1b[1P");
if (!terminalHarness.screen.lines[0].map((cell) => cell.character).join("").startsWith("Xbc")) {
    throw new Error("SSH terminal insert/delete character handling failed");
}
terminalHarness.feed("\x1b[?1049hwide:界\x1b[?1049l");
if (terminalHarness.screen.normalLines !== null ||
    terminalHarness.screen.lines[0].map((cell) => cell.character).join("").startsWith("wide:")) {
    throw new Error("SSH terminal alternate-screen restore failed");
}
console.log("WEB_CONSOLE_UI_TEST result=pass");
