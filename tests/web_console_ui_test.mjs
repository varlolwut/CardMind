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
const header = readFileSync(headerPath, "utf8").replace(/\r\n/g, "\n");
const routes = readFileSync(routesPath, "utf8").replace(/\r\n/g, "\n");
const stateBuilder = readFileSync(statePath, "utf8").replace(/\r\n/g, "\n");
const consoleSource = readFileSync(consoleSourcePath, "utf8").replace(/\r\n/g, "\n");
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
    'id="capabilitySummary"',
    'id="capabilityW"',
    'id="capabilityF"',
    'id="capabilityS"',
    'id="capabilityP"',
    'id="intentAuto" aria-pressed="true"',
    'id="intentNone" aria-pressed="false"',
    'id="intentWeb" aria-pressed="false"',
    'id="intentFiles" aria-pressed="false"',
    'id="intentSsh" aria-pressed="false"',
    'id="intentPython" aria-pressed="false"',
    'id="globalPolicyGrid"',
    'id="projectPolicyGrid"',
    'id="chatPolicyGrid"',
    'id="chatModel" maxlength="240"',
    'id="saveChatSettings"',
    'id="capabilityStateText"',
    'id="pendingToolDialog"',
    'id="pendingToolReason"',
    'id="pendingToolTarget"',
    'id="pendingToolPreview"',
    'id="pendingAllowOnce"',
    'id="pendingAllowChat"',
    'id="pendingDeny"',
    'id="pendingAcknowledge"',
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
    'id="toolActivity"',
    'id="refreshActivity"',
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
    'id="deleteProjectQuick"',
    'id="deleteChatQuick"',
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
    "'X-CardMind-Tool-Intent'",
    "'Content-Type':'application/vnd.cardmind.prompt-v1'",
    'function framedPromptBody(prompt,instructions)',
    "setMessageIntent('auto')",
    '/api/project/links?offset=',
    '/api/project/link',
    '/api/qr/show',
    '/api/qr/file?name=',
    '/api/chat/settings',
    '/api/pending',
    '/api/pending/allow-once',
    '/api/pending/allow-chat',
    '/api/pending/deny',
    '/api/pending/acknowledge',
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
    "const stateEndpoints={status:'/api/status',projects:'/api/projects',chats:'/api/chats',chat:'/api/chat',files:'/api/files',ssh:'/api/ssh/state',settings:'/api/settings',activity:'/api/activity',pending:'/api/pending'}",
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
    'function renderActivityState(s)',
    'function decodeToolPolicy(value,scoped)',
    'function encodeToolPolicy(values,scoped)',
    'function capabilityGroupState(capabilities,intent,group)',
    "'X-CardMind-Tool-Policy'",
    'master_tool_policy:',
    'new_chat_tool_policy:',
    '>Default model</label>',
    "event.type==='pending'",
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
    'id="sshToolsEnabled"',
    'id="savePermissions"',
    'data-intent="/search "',
    'data-intent="/file "',
    '/api/chat/permissions',
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

const policyStart = page.indexOf("const capabilityDefinitions=");
const policyEnd = page.indexOf("function createPolicySelect", policyStart);
if (policyStart < 0 || policyEnd < 0) {
    throw new Error("Cannot extract the fixed tool-policy codec");
}
const policyHarness = new Function(
    `${page.slice(policyStart, policyEnd)};return {` +
    "capabilityDefinitions,decodeToolPolicy,encodeToolPolicy," +
    "parseMessageIntent,capabilityGroupState,validatedCapabilities};",
)();
const masterPolicyValues = [
    "off", "ask", "allow", "off", "ask", "allow", "off", "ask",
];
const masterPolicy = "v1;ws=o;wf=q;fr=a;fw=o;sr=q;sm=a;sf=o;py=q";
if (policyHarness.encodeToolPolicy(masterPolicyValues, false) !== masterPolicy ||
    JSON.stringify(policyHarness.decodeToolPolicy(masterPolicy, false)) !==
        JSON.stringify(masterPolicyValues)) {
    throw new Error("Master tool policy does not round-trip through the fixed v1 codec");
}
const scopedPolicyValues = [
    "inherit", "off", "ask", "allow", "inherit", "off", "ask", "allow",
];
const scopedPolicy = "v1;ws=i;wf=o;fr=q;fw=a;sr=i;sm=o;sf=q;py=a";
if (policyHarness.encodeToolPolicy(scopedPolicyValues, true) !== scopedPolicy ||
    JSON.stringify(policyHarness.decodeToolPolicy(scopedPolicy, true)) !==
        JSON.stringify(scopedPolicyValues)) {
    throw new Error("Scoped tool policy does not round-trip through the fixed v1 codec");
}
for (const malformed of [
    "v1;wf=i;ws=o;fr=q;fw=a;sr=i;sm=o;sf=q;py=a",
    "v1;ws=i;wf=o;fr=q;fw=a;sr=i;sm=o;sf=q",
    "v1;ws=z;wf=o;fr=q;fw=a;sr=i;sm=o;sf=q;py=a",
]) {
    let rejected = false;
    try {
        policyHarness.decodeToolPolicy(malformed, true);
    } catch {
        rejected = true;
    }
    if (!rejected) {
        throw new Error(`Fixed tool-policy codec accepted malformed input: ${malformed}`);
    }
}
let masterInheritRejected = false;
try {
    policyHarness.decodeToolPolicy(
        "v1;ws=i;wf=o;fr=q;fw=a;sr=o;sm=o;sf=q;py=a",
        false,
    );
} catch {
    masterInheritRejected = true;
}
if (!masterInheritRejected) {
    throw new Error("Master tool policy accepted inherit");
}
const inheritedCapabilities = policyHarness.capabilityDefinitions.map((definition) => ({
    id: definition.id,
    raw_project: "inherit",
    raw_chat: "inherit",
    effective: "allow",
    source: "global",
}));
policyHarness.validatedCapabilities(inheritedCapabilities);
if (policyHarness.capabilityGroupState(inheritedCapabilities, "auto", 0) !== "inherit" ||
    policyHarness.capabilityGroupState(inheritedCapabilities, "none", 0) !== "off" ||
    policyHarness.capabilityGroupState(inheritedCapabilities, "required:1", 0) !==
        "required") {
    throw new Error("Capability group did not render Inherit, Off and Required deterministically");
}
const deniedInheritedCapabilities = structuredClone(inheritedCapabilities);
deniedInheritedCapabilities[0].effective = "deny";
if (policyHarness.capabilityGroupState(deniedInheritedCapabilities, "auto", 0) !== "off") {
    throw new Error("Inherited capability group ignored an effective parent denial");
}
const explicitWebCapabilities = inheritedCapabilities.map((capability) => ({...capability}));
explicitWebCapabilities[0].raw_chat = "allow";
explicitWebCapabilities[1].raw_chat = "allow";
if (policyHarness.capabilityGroupState(explicitWebCapabilities, "auto", 0) !== "allow") {
    throw new Error("Allowed Web capability group did not render Allow");
}
explicitWebCapabilities[0].effective = "ask";
if (policyHarness.capabilityGroupState(explicitWebCapabilities, "auto", 0) !== "ask") {
    throw new Error("Ask Web capability group did not render Ask");
}
explicitWebCapabilities[0].effective = "unavailable";
if (policyHarness.capabilityGroupState(explicitWebCapabilities, "auto", 0) !== "off") {
    throw new Error("Unavailable Web capability group did not render Off");
}
let capabilityOrderRejected = false;
try {
    policyHarness.validatedCapabilities([
        inheritedCapabilities[1], inheritedCapabilities[0],
        ...inheritedCapabilities.slice(2),
    ]);
} catch {
    capabilityOrderRejected = true;
}
if (!capabilityOrderRejected) {
    throw new Error("Capability state accepted out-of-order entries");
}

const intentRenderStart = page.indexOf("function renderIntentControls()");
const intentRenderEnd = page.indexOf("function renderCapabilityState()", intentRenderStart);
if (intentRenderStart < 0 || intentRenderEnd < 0) {
    throw new Error("Cannot extract the composer intent renderer");
}
function renderedIntentPresses(intent) {
    const ids = [
        "intentAuto", "intentNone", "intentWeb", "intentFiles", "intentSsh",
        "intentPython",
    ];
    const elements = Object.fromEntries(ids.map((id) => [
        `#${id}`,
        {value: "", setAttribute(name, value) { this[name] = value; }},
    ]));
    const renderIntentControls = new Function(
        "q", "parseMessageIntent", "messageIntent",
        `${page.slice(intentRenderStart, intentRenderEnd)};return renderIntentControls;`,
    )((selector) => elements[selector], policyHarness.parseMessageIntent, intent);
    renderIntentControls();
    return Object.fromEntries(ids.map((id) => [id, elements[`#${id}`]["aria-pressed"]]));
}
const autoPresses = renderedIntentPresses("auto");
if (autoPresses.intentAuto !== "true" ||
    Object.entries(autoPresses).some(([id, value]) => id !== "intentAuto" && value !== "false")) {
    throw new Error("Auto intent did not reset every composer aria-pressed state");
}
const requiredPresses = renderedIntentPresses("required:5");
if (requiredPresses.intentAuto !== "false" || requiredPresses.intentWeb !== "true" ||
    requiredPresses.intentFiles !== "false" || requiredPresses.intentSsh !== "true" ||
    requiredPresses.intentPython !== "false") {
    throw new Error("Required intent did not expose the selected capability groups");
}

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

const activityStart = page.indexOf("function renderActivityState(s)");
const activityEnd = page.indexOf("function renderProjectsState(s)", activityStart);
if (activityStart < 0 || activityEnd < 0) {
    throw new Error("Cannot extract the tool activity renderer");
}
const activityElement = {textContent: ""};
const renderActivityState = new Function(
    "q",
    `${page.slice(activityStart, activityEnd)};return renderActivityState;`,
)((selector) => selector === "#toolActivity" ? activityElement : null);
renderActivityState({activities: [{
    tool: "ssh_command",
    target: "selected_ssh",
    status: "succeeded",
    duration_ms: 123,
    output_bytes: 45,
    exit_status: 7,
}]});
for (const value of ["ssh_command", "selected_ssh", "succeeded", "123 ms", "45 B", "exit 7"]) {
    if (!activityElement.textContent.includes(value)) {
        throw new Error(`Tool activity renderer omitted ${value}`);
    }
}
let invalidActivityRejected = false;
try {
    renderActivityState({activities: [{tool: "read_file"}]});
} catch {
    invalidActivityRejected = true;
}
if (!invalidActivityRejected) {
    throw new Error("Tool activity renderer accepted an incomplete row");
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
if (promptStreamCompletionError("", "pending", "Waiting for confirmation") !== "") {
    throw new Error("A pending SSE terminal event was rejected");
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
const summaryWithPending = summarizeSseEvents([{
    type: "pending",
    message: "Waiting for confirmation",
}], "", "");
if (summaryWithPending.terminalType !== "pending" ||
    summaryWithPending.streamError !== "Waiting for confirmation") {
    throw new Error("A pending SSE event was not treated as terminal");
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
    "/api/activity",
    "/api/storage/confirm",
    "/api/projects",
    "/api/chats",
    "/api/chat",
    "/api/files",
    "/api/ssh/state",
    "/api/settings",
    "/api/pending",
    "/api/pending/allow-once",
    "/api/pending/allow-chat",
    "/api/pending/deny",
    "/api/pending/acknowledge",
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
    "decodeToolMessageIntent(",
    "resolveChatToolRequestPlan(",
    "toolPlan.schemas != 0",
    "routeProjectToolCall(\n                      consoleSettings, toolPlan",
    "toolStorageReadable, toolStorageWritable, toolStorageWritable",
    "failedWebRequestIntent = toolPlan.intent",
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
const promptToolPlan = promptSubmitSource.indexOf("resolveChatToolRequestPlan(");
const requiredToolReject = promptSubmitSource.indexOf(
    "toolPlan.missingRequiredGroups != 0", promptToolPlan);
const promptAppend = promptSubmitSource.indexOf("appendProjectChatMessages(");
const promptAppendFailure = promptSubmitSource.indexOf(
    "if (!saved.success)", promptAppend);
const retryInstructionsStage = promptSubmitSource.indexOf(
    "failedWebRequestInstructions = std::move(requestInstructions)", promptAppendFailure);
const retryOutputStage = promptSubmitSource.indexOf(
    "failedWebRequestOutputTokens = submittedPolicy.maximumOutputTokens",
    retryInstructionsStage);
const retryIntentStage = promptSubmitSource.indexOf(
    "failedWebRequestIntent = intent", retryOutputStage);
const firstMessageMetadataSave = promptSubmitSource.indexOf(
    "saveProjectChatMetadata(activeChat)", retryIntentStage);
const acceptedMetadataFailureStream = promptSubmitSource.indexOf(
    'server.send(200, "text/event-stream; charset=utf-8", "")',
    firstMessageMetadataSave);
const acceptedMetadataFailureEvent = promptSubmitSource.indexOf(
    'sendWebSse(server, "error", "", saved.error)',
    acceptedMetadataFailureStream);
if (promptSubmitStart < 0 || promptSubmitEnd < 0 || promptAppend < 0 ||
    promptToolPlan < 0 || requiredToolReject < promptToolPlan ||
    promptAppend < requiredToolReject ||
    promptAppendFailure < promptAppend || retryInstructionsStage < promptAppendFailure ||
    retryOutputStage < retryInstructionsStage ||
    retryIntentStage < retryOutputStage || firstMessageMetadataSave < retryIntentStage ||
    acceptedMetadataFailureStream < firstMessageMetadataSave ||
    acceptedMetadataFailureEvent < acceptedMetadataFailureStream) {
    throw new Error(
        "Web tool plan or retry snapshot is not ordered around durable prompt append");
}
for (const [handler, nextHandler] of [
    ["void handleDuplicateProject()", "void handleArchiveProject()"],
    ["void handleDeleteProject()", "void handleProjectLinks()"],
    ["void handleDuplicateChat()", "void handleExportChat()"],
    ["void handleImportChatBundle()", "void handleDeleteChat()"],
    ["void handleDeleteChat()", "void handleSettings()"],
]) {
    const start = consoleSource.indexOf(handler);
    const end = consoleSource.indexOf(nextHandler, start);
    const source = consoleSource.slice(start, end);
    const clear = source.indexOf("clearFailedWebRequestInstructions();");
    const success = source.indexOf("sendWebJson(server, 200", clear);
    if (start < 0 || end < 0 || clear < 0 || success < clear) {
        throw new Error(`Successful ownership change keeps stale retry state: ${handler}`);
    }
}
const retryHandlerStart = consoleSource.indexOf("void handlePromptRetry()");
const retryHandlerEnd = consoleSource.indexOf(
    "void handleSelectProject()", retryHandlerStart);
const retryHandlerSource = consoleSource.slice(retryHandlerStart, retryHandlerEnd);
const retryIntentSnapshot = retryHandlerSource.indexOf("failedWebRequestIntent");
const retryToolPlan = retryHandlerSource.indexOf(
    "resolveChatToolRequestPlan(", retryIntentSnapshot);
const retryToolStream = retryHandlerSource.indexOf(
    "streamStoredWebPrompt(", retryToolPlan);
if (retryHandlerStart < 0 || retryHandlerEnd < 0 || retryIntentSnapshot < 0 ||
    retryToolPlan < retryIntentSnapshot || retryToolStream < retryToolPlan ||
    retryHandlerSource.includes("kToolIntentHeader")) {
    throw new Error("Web retry does not reuse its accepted tool intent snapshot");
}
const webSendStart = page.indexOf("async function sendPrompt()");
const webSendEnd = page.indexOf("async function retryFailedPrompt()", webSendStart);
const webSendSource = page.slice(webSendStart, webSendEnd);
const webPromptAccepted = webSendSource.indexOf("await rawPrompt(");
const webIntentReset = webSendSource.indexOf("setMessageIntent('auto')", webPromptAccepted);
if (webSendStart < 0 || webSendEnd < 0 || webPromptAccepted < 0 ||
    webIntentReset < webPromptAccepted) {
    throw new Error("Web composer intent does not reset to Auto after prompt acceptance");
}
const webRetryStart = page.indexOf("async function retryFailedPrompt()");
const webRetryEnd = page.indexOf("q('#send').onclick", webRetryStart);
if (webRetryStart < 0 || webRetryEnd < 0 ||
    page.slice(webRetryStart, webRetryEnd).includes("messageIntent")) {
    throw new Error("Web retry reads or changes the current composer intent");
}
for (const acceptedOperation of [
    "await post('/api/project/select',{id:q('#projects').value});setMessageIntent('auto')",
    "await post('/api/project/new',{title});setMessageIntent('auto')",
    "await request('/api/chat/select',{method:'POST',body:new URLSearchParams({id:q('#chats').value})});setMessageIntent('auto')",
    "await request('/api/chat/new',{method:'POST'});setMessageIntent('auto')",
    "await post('/api/project/duplicate',{title});setMessageIntent('auto')",
    "await post('/api/project/delete',{});setMessageIntent('auto')",
    "await post('/api/chat/duplicate',{});setMessageIntent('auto')",
    "await post('/api/chat/import',{name});setMessageIntent('auto')",
    "await post('/api/chat/delete',{});setMessageIntent('auto')",
    "await post('/api/chat/clear',{});setMessageIntent('auto')",
]) {
    if (!page.includes(acceptedOperation)) {
        throw new Error(`Web composer intent is not reset after ${acceptedOperation}`);
    }
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
