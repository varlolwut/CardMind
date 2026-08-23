import { createServer } from "node:http";
import { readFileSync } from "node:fs";

const sourcePath = new URL("../firmware/CardputerAssistant/src/web_console.cpp", import.meta.url);
const source = readFileSync(sourcePath, "utf8");
const functionStart = source.indexOf("void sendConsolePage()\n{");
const functionEnd = source.indexOf("\nvoid sendLoginPage()", functionStart);
if (functionStart < 0 || functionEnd < 0) {
    throw new Error("Could not locate sendConsolePage in web_console.cpp");
}

const literals = [...source.slice(functionStart, functionEnd).matchAll(/R"HTML\(([\s\S]*?)\)HTML"/g)];
if (literals.length !== 2) {
    throw new Error(`Expected two console HTML literals, found ${literals.length}`);
}

const previewState = {
    ip: "192.168.1.129",
    battery: 87,
    wifi_rssi: -53,
    free_heap: 124608,
    largest_heap: 63488,
    sd_used_bytes: 3145728,
    sd_total_bytes: 31914983424,
    active_chat_id: "chat-1",
    active_chat_title: "CardMind UX review",
    instructions: "Answer clearly and keep code examples compact.",
    status: "",
    model: "claude-sonnet-4-6",
    api_base_url: "https://api.example.com",
    chats: [
        {id: "chat-1", title: "CardMind UX review", pinned: true, archived: false, total_messages: 18},
        {id: "chat-2", title: "Travel notes", pinned: false, archived: false, total_messages: 9},
        {id: "chat-3", title: "SSH troubleshooting", pinned: false, archived: false, total_messages: 14},
    ],
    messages: [
        {role: "user", content: "Can you summarize the last device regression?"},
        {role: "assistant", content: "All storage, network, streaming, voice, search, and SSH checks passed. Peak free heap remained stable after the Web console cycle."},
        {role: "user", content: "Show me the remaining UX work."},
        {role: "assistant", content: "The next pass focuses on navigation clarity, mobile ergonomics, and reducing heap allocations while the browser console is open."},
    ],
    ssh_profiles: [
        {name: "Home server", username: "cardmind", host: "192.168.1.40", port: 22, auth_mode: "key"},
    ],
    ssh_selected: 0,
    ssh_name: "Home server",
    ssh_host: "192.168.1.40",
    ssh_port: 22,
    ssh_username: "cardmind",
    ssh_auth_mode: "key",
    ssh_terminal_open: false,
    files: [
        {name: "notes.md", size: 18422},
        {name: "report.txt", size: 92480},
        {name: "settings.json", size: 1482},
    ],
};

const stateScript = `const previewState=${JSON.stringify(previewState)};\n`;
const scriptEnd = literals[1][1].replace(";refresh();", ";render(previewState);");
const page = `${literals[0][1]}const csrf='preview';\n${stateScript}${scriptEnd}`;
const server = createServer((request, response) => {
    if (request.url !== "/" && request.url !== "/index.html") {
        response.writeHead(404, {"Content-Type": "text/plain; charset=utf-8"});
        response.end("Not found");
        return;
    }
    response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Type": "text/html; charset=utf-8",
    });
    response.end(page);
});

server.listen(8765, "127.0.0.1", () => {
    console.log("WEB_CONSOLE_PREVIEW address=http://127.0.0.1:8765/");
});
