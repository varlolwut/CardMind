import { readFileSync } from "node:fs";

const sourcePath = new URL("../firmware/CardputerAssistant/src/web_console.cpp", import.meta.url);
const source = readFileSync(sourcePath, "utf8");
const functionStart = source.indexOf("String consolePage()\n{");
const functionEnd = source.indexOf("\nvoid sendLoginPage()", functionStart);

if (functionStart < 0 || functionEnd < 0) {
    throw new Error("Could not locate consolePage in web_console.cpp");
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
    'id="sshTerminal"',
    'id="diagnostics"',
];

for (const fragment of requiredFragments) {
    if (!page.includes(fragment)) {
        throw new Error(`Web console page is missing ${fragment}`);
    }
}

const scriptMatch = page.match(/<script>([\s\S]*)<\/script>/);
if (scriptMatch === null) {
    throw new Error("Web console page does not contain a script block");
}

new Function(scriptMatch[1]);
console.log("WEB_CONSOLE_UI_TEST result=pass");
