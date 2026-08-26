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
const consoleSourcePath = new URL(
    "../firmware/CardputerAssistant/src/web_console.cpp",
    import.meta.url,
);
const page = readFileSync(pagePath, "utf8").replace(/\r\n/g, "\n");
const header = readFileSync(headerPath, "utf8");
const routes = readFileSync(routesPath, "utf8");
const stateBuilder = readFileSync(statePath, "utf8");
const consoleSource = readFileSync(consoleSourcePath, "utf8");
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
    'id="projectChatHistoryQuotaMiB"',
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
    'id="sdDegradedBanner"',
    'id="sdDegradedTitle"',
    'id="sdDegradedMessage"',
    'id="confirmSdReplacement"',
    'id="projects"',
    'id="newProject"',
    "loadMoreProjectsButton.id='loadMoreProjects'",
    'id="projectInstructions"',
    'id="projectContextBudget"',
    'id="projectOutputTokens"',
    'id="requestOutputTokens"',
    'id="requestInstructions"',
    'id="compactChat"',
    'history_before_messages',
    'archiveCursorBytes',
    'archivedLoadedMessages',
    'function validatedArchivedPage(value,currentCursor)',
    'editableFiles=new Map()',
    "typeof f.editable!=='boolean'",
    "q('#openFile').disabled=!editable",
    "q('#qrFromFile').disabled=!editable",
    "Binary file linked for transfer only; it is not exposed to the model.",
    'function resetFileEditorSelection()',
    "fileName&&q('#workspaceFiles').value!==fileName)resetFileEditorSelection()",
    "generation!==fileOpenGeneration||q('#workspaceFiles').value!==selected",
    "selected!==fileName",
    "finally{if(generation===fileOpenGeneration)q('#saveFile').disabled",
    "q('#workspaceFiles').onchange=()=>{resetFileEditorSelection();updateProjectLinkControl()}",
    "result.summary_event!=='manual_regenerated'",
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
    '/api/project/settings/raw',
    '/api/chat/instructions/raw',
    '/api/prompt/raw',
    "'Content-Type':'text/plain; charset=utf-8'",
    "'X-CardMind-Model-Encoded'",
    "'X-CardMind-Context-Bytes'",
    "'X-CardMind-Output-Tokens'",
    "'X-CardMind-Auto-Compact'",
    "'Content-Type':'application/vnd.cardmind.prompt-v1'",
    'function framedPromptBody(prompt,instructions)',
    '/api/project/links?offset=',
    '/api/project/link',
    '/api/qr/show',
    '/api/qr/file?name=',
    '/api/chat/permissions',
    '/api/chat/archived',
    '/api/chat/clear',
    '/api/console/close',
    '/api/storage/confirm',
    "x.address+'handoff?token='",
    'handoff_token',
    'sshPollBusy',
    'output_base64',
    "event.type==='notice'",
    "userMessage.textContent='You: '+prompt",
    "outputTokens||'0'",
    "'/api/prompt/retry'",
    "'/api/chat/compact'",
    'retryFailedPrompt',
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
    'function renderSdDegradedState(s)',
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
    "request('/api/chat/instructions',{method:'POST',body:new URLSearchParams",
    "request('/api/prompt',{method:'POST',body:new URLSearchParams",
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

const degradedStart = page.indexOf("function renderSdDegradedState(s)");
const degradedEnd = page.indexOf("function renderStatusState(s)", degradedStart);
if (degradedStart < 0 || degradedEnd < 0) {
    throw new Error("Cannot extract the microSD degraded-state renderer");
}
const degradedElements = {
    "#sdDegradedBanner": { hidden: true },
    "#confirmSdReplacement": { hidden: true },
    "#sdDegradedTitle": { textContent: "" },
    "#sdDegradedMessage": { textContent: "" },
};
const renderSdDegradedState = new Function(
    "q",
    `${page.slice(degradedStart, degradedEnd)};return renderSdDegradedState;`,
)((selector) => degradedElements[selector]);
renderSdDegradedState({
    sd_state: "replaced",
    sd_error_code: "micro_sd_replaced",
    sd_error: "Different microSD detected",
});
if (degradedElements["#sdDegradedBanner"].hidden ||
    degradedElements["#confirmSdReplacement"].hidden ||
    degradedElements["#sdDegradedMessage"].textContent !== "Different microSD detected") {
    throw new Error("Replacement state did not expose the stable confirmation banner");
}
renderSdDegradedState({sd_state: "ready"});
if (!degradedElements["#sdDegradedBanner"].hidden) {
    throw new Error("Ready storage left the degraded-state banner visible");
}

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
    ";\nasync function consumePromptStream",
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
    "/api/storage/confirm",
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
for (const quotaFragment of [
    'document["project_chat_history_quota_bytes"]',
    'settings.projectChatHistoryQuotaBytes',
]) {
    if (!stateBuilder.includes(quotaFragment)) {
        throw new Error(`Web settings quota state is incomplete: ${quotaFragment}`);
    }
}
const settingsHandlerStart = consoleSource.indexOf("void handleSettings()");
const settingsHandlerEnd = consoleSource.indexOf(
    "void handleClearChat()", settingsHandlerStart);
const settingsHandlerSource = consoleSource.slice(settingsHandlerStart, settingsHandlerEnd);
const quotaAssignment = settingsHandlerSource.indexOf(
    "updated.projectChatHistoryQuotaBytes = historyQuotaMiB * 1024U * 1024U");
const settingsSave = settingsHandlerSource.indexOf("saveSettings(updated)");
if (settingsHandlerStart < 0 || settingsHandlerEnd < 0 || quotaAssignment < 0 ||
    settingsSave < quotaAssignment) {
    throw new Error("Web settings quota is not assigned before the settings save");
}
if (!stateBuilder.includes(
    'item["editable"] = isWorkspaceTextFile(std::string(file.name.c_str()))')) {
    throw new Error("Web file state does not expose the central text-file policy");
}
for (const boundary of ["void handleQrFile()", "void handleFileRead()", "void handleFileSave()"]) {
    const start = consoleSource.indexOf(boundary);
    const end = consoleSource.indexOf("\nvoid ", start + boundary.length);
    if (start < 0 || end < 0 || !consoleSource.slice(start, end).includes(
        "isWorkspaceTextFile(std::string(name.c_str()))")) {
        throw new Error(`${boundary} does not guard the central workspace text policy`);
    }
}
for (const quotaFragment of [
    'server.arg("project_chat_history_quota_mib")',
    'historyQuotaMiB < 2 || historyQuotaMiB > 4095',
    'updated.projectChatHistoryQuotaBytes = historyQuotaMiB * 1024U * 1024U',
    'consoleSettings.projectChatHistoryQuotaBytes',
]) {
    if (!consoleSource.includes(quotaFragment)) {
        throw new Error(`Web settings quota behavior is incomplete: ${quotaFragment}`);
    }
}
for (const field of [
    'document["sd_state"]',
    'document["sd_error_code"]',
    'document["sd_error"]',
    'document["sd_readable"]',
    'document["sd_writable"]',
    'document["sd_free_bytes"]',
]) {
    if (!stateBuilder.includes(field)) {
        throw new Error(`Web Console status omits typed microSD field: ${field}`);
    }
}
if (!consoleSource.includes("handleStorageConfirm") ||
    !consoleSource.includes("confirmSdStorageReplacement()") ||
    !consoleSource.includes("allowWebStorageRoute")) {
    throw new Error("Web Console storage confirmation or production route guard is missing");
}
if (!page.includes("Restart CardMind to initialize the confirmed workspace.") ||
    !consoleSource.includes(
        "microSD replacement confirmed; restart CardMind to initialize the workspace")) {
    throw new Error("microSD replacement confirmation does not require an explicit restart");
}
const storageAccessStart = consoleSource.indexOf("WebStorageAccess webStorageAccessForRoute");
const storageAccessEnd = consoleSource.indexOf("int webStorageErrorStatus", storageAccessStart);
const storageAccess = consoleSource.slice(storageAccessStart, storageAccessEnd);
for (const writeRoute of ["SshStart", "SshTrust"]) {
    if (!storageAccess.includes(`case WebConsoleRouteHandler::${writeRoute}:`)) {
        throw new Error(`Web Console storage guard omits the SSH write route ${writeRoute}`);
    }
}
const readOnlyLoadStart = consoleSource.indexOf("OperationResult loadCommittedConsoleStorageReadOnly()");
const readOnlyLoadEnd = consoleSource.indexOf(
    "std::string effectiveProjectChatInstructions", readOnlyLoadStart,
);
const readOnlyLoad = consoleSource.slice(readOnlyLoadStart, readOnlyLoadEnd);
if (readOnlyLoadStart < 0 || !readOnlyLoad.includes("loadProjectStorageManifest()") ||
    !readOnlyLoad.includes("loadProject(") || !readOnlyLoad.includes("loadActiveChat(") ||
    !readOnlyLoad.includes("refreshProjects()") || !readOnlyLoad.includes("refreshChats()") ||
    !readOnlyLoad.includes("refreshFiles()")) {
    throw new Error("Full microSD startup does not load the committed project/chat/file state");
}
for (const forbiddenWrite of [
    "selectActiveProject(", "saveActiveChatSelection(", "createProjectChat(",
    "saveProjectStorageManifest(", "saveProject(",
]) {
    if (readOnlyLoad.includes(forbiddenWrite)) {
        throw new Error(`Full microSD startup calls a write path: ${forbiddenWrite}`);
    }
}
if (!consoleSource.includes(
    "startupStorage.state == SdStorageState::Full) {\n        result = loadCommittedConsoleStorageReadOnly();")) {
    throw new Error("Full microSD startup does not use the read-only committed-state loader");
}
const keyUploadStart = consoleSource.indexOf("void handleSshKeyUploadData()");
const keyUploadEnd = consoleSource.indexOf("void handleSshKeyUploadComplete()", keyUploadStart);
const keyUpload = consoleSource.slice(keyUploadStart, keyUploadEnd);
if ((keyUpload.match(/requireSdWriteAccess\(/g) ?? []).length < 5 ||
    !keyUpload.includes("if (requireSdWriteAccess(0, 0).success)")) {
    throw new Error("SSH private-key upload is not guarded across chunks, flush and install");
}
for (const rawRoute of [
    ["/api/project/settings/raw", "ProjectSettingsRawComplete", "ProjectSettingsRawData"],
    ["/api/chat/instructions/raw", "InstructionsRawComplete", "InstructionsRawData"],
    ["/api/prompt/raw", "PromptRawComplete", "PromptRawData"],
]) {
    const [endpoint, completeHandler, dataHandler] = rawRoute;
    const routeStart = routes.indexOf(`server.on("${endpoint}"`);
    const routeEnd = routes.indexOf(");", routeStart);
    const registration = routes.slice(routeStart, routeEnd);
    if (routeStart < 0 || !registration.includes(completeHandler) ||
        !registration.includes(dataHandler)) {
        throw new Error(`Raw Web Console route is incomplete: ${endpoint}`);
    }
}
for (const binding of [
    "handleProjectSettings,\n            handleProjectSettingsRawComplete,\n            handleProjectSettingsRawData",
    "handlePrompt,\n            handlePromptRawComplete,\n            handlePromptRawData",
    "handleInstructions,\n            handleInstructionsRawComplete,\n            handleInstructionsRawData",
]) {
    if (!consoleSource.includes(binding)) {
        throw new Error(`Raw handler dispatch table is incomplete: ${binding}`);
    }
}
for (const requestPolicyFragment of [
    "activeProject, storedChat, requestInstructions",
    "resolveProjectRequestPolicy(",
    "shouldAutomaticallyCompactRequest(",
    "failedWebRequestInstructions = std::move(requestInstructions)",
    "failedWebRequestOutputTokens = requestPolicy.maximumOutputTokens",
    "requestedOutputTokens.isEmpty() && retryMatchesFailedRequest",
    "outputBudget = {true, failedWebRequestOutputTokens, \"\"}",
    "failedWebRequestOutputTokens = 0",
    "clearFailedWebRequestInstructions();",
    "application/vnd.cardmind.prompt-v1",
    "kMaximumRequestInstructionsBytes",
]) {
    if (!consoleSource.includes(requestPolicyFragment)) {
        throw new Error(`Web prompt request policy is incomplete: ${requestPolicyFragment}`);
    }
}
const clearChatStart = consoleSource.indexOf("void handleClearChat()");
const clearChatEnd = consoleSource.indexOf("void handleArchivedMessages()", clearChatStart);
if (clearChatStart < 0 || clearChatEnd < 0 ||
    !consoleSource.slice(clearChatStart, clearChatEnd).includes(
        "clearFailedWebRequestInstructions();")) {
    throw new Error("Successful Web chat clear does not discard failed request retry state");
}
const promptSubmitStart = consoleSource.indexOf("void processWebPrompt(");
const promptSubmitEnd = consoleSource.indexOf(
    "void handlePromptRawData()", promptSubmitStart);
const promptSubmitSource = consoleSource.slice(promptSubmitStart, promptSubmitEnd);
const promptAppend = promptSubmitSource.indexOf("appendProjectChatMessages(");
const promptAppendFailure = promptSubmitSource.indexOf(
    "if (!saved.success)", promptAppend);
const retryInstructionsStage = promptSubmitSource.indexOf(
    "failedWebRequestInstructions = std::move(requestInstructions)", promptAppendFailure);
const retryOutputStage = promptSubmitSource.indexOf(
    "failedWebRequestOutputTokens = submittedPolicy.maximumOutputTokens",
    retryInstructionsStage);
const firstMessageMetadataSave = promptSubmitSource.indexOf(
    "saveProjectChatMetadata(activeChat)", retryOutputStage);
if (promptSubmitStart < 0 || promptSubmitEnd < 0 || promptAppend < 0 ||
    promptAppendFailure < promptAppend || retryInstructionsStage < promptAppendFailure ||
    retryOutputStage < retryInstructionsStage ||
    firstMessageMetadataSave < retryOutputStage) {
    throw new Error(
        "Web prompt retry state is not staged after append success and before metadata save");
}
for (const summaryFragment of [
    "readProjectChatMessagesByIndex(",
    "std::uint32_t nextMessageIndex = 0",
    "std::string stagedSummary",
    "current.chat.contextSummary = std::move(stagedSummary)",
    'document["summary_event"] = "manual_regenerated"',
    "Context summary updated automatically; raw history was preserved",
]) {
    if (!consoleSource.includes(summaryFragment)) {
        throw new Error(`Web manual/automatic summary path is incomplete: ${summaryFragment}`);
    }
}
const compactStart = consoleSource.indexOf("void handleChatCompact()");
const compactEnd = consoleSource.indexOf("void handleChatPermissions()", compactStart);
const compactSource = consoleSource.slice(compactStart, compactEnd);
const compactPager = compactSource.indexOf("readProjectChatMessagesByIndex(");
const compactMetadataReload = compactSource.indexOf("ChatDocumentResult current", compactPager);
const compactMetadataSave = compactSource.indexOf(
    "saveProjectChatMetadata(current.chat)", compactMetadataReload);
if (compactStart < 0 || compactEnd < 0 || compactPager < 0 ||
    compactMetadataReload < compactPager || compactMetadataSave < compactMetadataReload ||
    compactSource.includes("Context summary is already up to date")) {
    throw new Error(
        "Manual summary is not staged across indexed raw pages before one metadata save");
}
if (!stateBuilder.includes("resolveContextUsage(") ||
    !stateBuilder.includes('document["active_context_messages"] = contextUsage.retainedMessages') ||
    !stateBuilder.includes('document["active_context_bytes"] = contextUsage.retainedBytes')) {
    throw new Error("Web chat state does not use shared production context semantics");
}
if (page.includes("Math.min(x.next_offset,historyBefore") ||
    !page.includes("x.messages.slice(0,remaining)") ||
    !page.includes("value.next_offset<=currentCursor")) {
    throw new Error("Archived history UI mixes message counts with its opaque byte cursor");
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
