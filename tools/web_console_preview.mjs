import { createServer } from "node:http";
import { readFileSync } from "node:fs";

const pagePath = new URL(
    "../firmware/CardputerAssistant/assets/web_console.html",
    import.meta.url,
);
const source = readFileSync(pagePath, "utf8");

const previewState = {
    ip: "192.168.1.129",
    battery: 87,
    wifi_rssi: -53,
    free_heap: 124608,
    minimum_heap: 98240,
    largest_heap: 63488,
    stack_free: 4288,
    cpu_mhz: 240,
    uptime_ms: 4368000,
    reset_reason: 1,
    sd_used_bytes: 3145728,
    sd_total_bytes: 31914983424,
    active_chat_id: "chat-1",
    active_chat_title: "CardMind UX review",
    active_context_messages: 18,
    active_context_bytes: 12480,
    maximum_context_messages: 64,
    maximum_context_bytes: 32768,
    archived_messages: 6,
    instructions: "Answer clearly and keep code examples compact.",
    ssh_tools_enabled: false,
    status: "",
    firmware_version: "1.12.0-preview",
    wifi_ssid: "Studio 2.4 GHz",
    model: "claude-sonnet-4-6",
    api_base_url: "https://api.example.com",
    api_key_configured: true,
    stt_base_url: "https://speech.example.com",
    stt_model: "whisper-1",
    stt_key_configured: true,
    search_base_url: "https://search.example.com",
    search_key_configured: true,
    tts_base_url: "https://speech.example.com",
    tts_model: "tts-1",
    tts_voice: "alloy",
    tts_key_configured: true,
    tts_auto_play: false,
    tts_volume: 190,
    display_brightness: 180,
    screen_sleep_minutes: 5,
    keyboard_repeat_ms: 125,
    power_profile: 1,
    python_layout_ready: true,
    python_image_ready: true,
    python_error: "",
    python_runtime_error: "",
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
    ssh_key_installed: true,
    ssh_configured: true,
    files: [
        {name: "notes.md", size: 18422},
        {name: "report.txt", size: 92480},
        {name: "settings.json", size: 1482},
    ],
};

const stateScript = `const previewState=${JSON.stringify(previewState)};\n`;
const page = source
    .replace("let csrf=''", "let csrf='preview'")
    .replace("</script>", `${stateScript}render(previewState);</script>`)
    .replace("loadSession().then(refresh).catch(e=>showStatus(e.message));", "");
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
