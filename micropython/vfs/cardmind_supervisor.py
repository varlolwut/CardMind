import _thread
import binascii
import esp32
import gc
import hashlib
import json
import machine
import network
import os
import socket
import sys
import time
import vfs


_CONFIG_NAMESPACE = "cardmind_py"
_CARDMIND_LABEL = "cardmind"
_UPDATE_PATH = "/sd/assistant/update.bin"
_SCRIPT_ROOT = "/sd/assistant/files"
_MAXIMUM_SCRIPT_BYTES = 65536
_MAXIMUM_OUTPUT_BYTES = 16384
_HTTP_LIMIT_BYTES = 73728
_SESSION_SECONDS = 900

_state_lock = _thread.allocate_lock()
_script_running = False
_script_name = ""
_script_output = ""
_session_token = ""
_session_seen_at = 0


def _read_blob(namespace, key, maximum_bytes):
    value = bytearray(maximum_bytes)
    length = namespace.get_blob(key, value)
    return bytes(value[:length]).rstrip(b"\x00").decode("utf-8")


def _try_read_blob(namespace, key, maximum_bytes):
    try:
        return _read_blob(namespace, key, maximum_bytes)
    except OSError:
        return ""


def _write_blob(namespace, key, value):
    namespace.set_blob(key, value.encode("utf-8") + b"\x00")
    namespace.commit()


def _clear_key(namespace, key):
    try:
        namespace.erase_key(key)
    except OSError as error:
        if error.args[0] != -4354:
            raise


def _mount_sd():
    sd = machine.SDCard(
        slot=2,
        sck=machine.Pin(40),
        miso=machine.Pin(39),
        mosi=machine.Pin(14),
        cs=machine.Pin(12),
        freq=20000000,
    )
    vfs.mount(sd, "/sd")
    for directory in ("/sd/assistant", _SCRIPT_ROOT):
        try:
            os.mkdir(directory)
        except OSError as error:
            if error.args[0] != 17:
                raise
    return sd


def _cardmind_partition():
    matches = esp32.Partition.find(esp32.Partition.TYPE_APP, label=_CARDMIND_LABEL)
    if len(matches) != 1:
        raise RuntimeError("CardMind app partition is missing or ambiguous")
    return matches[0]


def _apply_pending_update(namespace):
    try:
        pending = namespace.get_i32("upd_pending")
    except OSError:
        return False
    if pending != 1:
        return False
    expected_size = namespace.get_i32("upd_size")
    expected_sha = _read_blob(namespace, "upd_sha", 65)
    if expected_size <= 0 or len(expected_sha) != 64:
        raise RuntimeError("Pending CardMind update metadata is invalid")
    source = os.stat(_UPDATE_PATH)
    if source[6] != expected_size:
        raise RuntimeError("Pending CardMind update file has the wrong size")
    partition = _cardmind_partition()
    partition_size = partition.info()[3]
    if expected_size > partition_size:
        raise RuntimeError("Pending CardMind update exceeds its app partition")
    digest = hashlib.sha256()
    written = 0
    block_index = 0
    with open(_UPDATE_PATH, "rb") as update:
        first = update.read(1)
        if first != b"\xe9":
            raise RuntimeError("Pending CardMind update is not an ESP application image")
        update.seek(0)
        while written < expected_size:
            chunk = update.read(min(4096, expected_size - written))
            if not chunk:
                raise RuntimeError("Pending CardMind update ended before the declared size")
            digest.update(chunk)
            padded = chunk if len(chunk) == 4096 else chunk + b"\xff" * (4096 - len(chunk))
            partition.writeblocks(block_index, padded)
            written += len(chunk)
            block_index += 1
            if block_index % 32 == 0:
                print("PYUPDATE write_bytes={}".format(written))
    actual_sha = binascii.hexlify(digest.digest()).decode()
    if written != expected_size or actual_sha != expected_sha:
        raise RuntimeError("CardMind update failed source SHA-256 verification")
    verify_digest = hashlib.sha256()
    verified = 0
    block_index = 0
    buffer = bytearray(4096)
    while verified < expected_size:
        partition.readblocks(block_index, buffer)
        take = min(4096, expected_size - verified)
        verify_digest.update(memoryview(buffer)[:take])
        verified += take
        block_index += 1
    verified_sha = binascii.hexlify(verify_digest.digest()).decode()
    if verified_sha != expected_sha:
        raise RuntimeError("CardMind update failed flash SHA-256 verification")
    _clear_key(namespace, "upd_pending")
    _clear_key(namespace, "upd_size")
    _clear_key(namespace, "upd_sha")
    _clear_key(namespace, "upd_error")
    namespace.commit()
    os.remove(_UPDATE_PATH)
    partition.set_boot()
    print("PYUPDATE result=pass bytes={}".format(verified))
    machine.reset()
    return True


def _remember_update_error(namespace, error):
    message = str(error)
    if len(message) > 190:
        message = message[:190]
    _write_blob(namespace, "upd_error", message)
    print("PYUPDATE result=failed error={}".format(message))


def recover_to_cardmind(error):
    message = str(error)
    if len(message) > 190:
        message = message[:190]
    namespace = esp32.NVS(_CONFIG_NAMESPACE)
    _write_blob(namespace, "mode_error", message)
    print("PYTHON_MODE result=failed error={}".format(message))
    time.sleep_ms(1000)
    _cardmind_partition().set_boot()
    machine.reset()


def _connect_wifi(namespace):
    ssid = _read_blob(namespace, "ssid", 65)
    password = _read_blob(namespace, "wifi_pass", 193)
    if not ssid:
        raise RuntimeError("Python mode has no synchronized Wi-Fi SSID")
    station = network.WLAN(network.WLAN.IF_STA)
    station.active(True)
    if not station.isconnected():
        station.connect(ssid, password)
        deadline = time.ticks_add(time.ticks_ms(), 20000)
        while not station.isconnected() and time.ticks_diff(deadline, time.ticks_ms()) > 0:
            time.sleep_ms(100)
    if not station.isconnected():
        raise RuntimeError("Python mode failed to connect to synchronized Wi-Fi")
    return station.ifconfig()[0]


def _safe_script_name(value):
    if not value or len(value) > 64 or not value.endswith(".py"):
        return False
    if value in (".", "..") or "/" in value or "\\" in value:
        return False
    for character in value:
        if not (
            "a" <= character <= "z"
            or "A" <= character <= "Z"
            or "0" <= character <= "9"
            or character in "._-"
        ):
            return False
    return True


def _script_path(name):
    if not _safe_script_name(name):
        raise ValueError("Script name must contain only letters, digits, dot, dash, underscore and end in .py")
    return _SCRIPT_ROOT + "/" + name


def _list_scripts():
    output = []
    for name in os.listdir(_SCRIPT_ROOT):
        if _safe_script_name(name):
            details = os.stat(_script_path(name))
            if details[0] & 0x4000 == 0:
                output.append({"name": name, "bytes": details[6]})
    output.sort(key=lambda item: item["name"].lower())
    return output


def _append_output(text):
    global _script_output
    with _state_lock:
        _script_output += text
        if len(_script_output) > _MAXIMUM_OUTPUT_BYTES:
            _script_output = _script_output[-_MAXIMUM_OUTPUT_BYTES:]


def _captured_print(*values, **options):
    separator = options.get("sep", " ")
    ending = options.get("end", "\n")
    _append_output(separator.join(str(value) for value in values) + ending)


class _OutputWriter:
    def write(self, value):
        _append_output(str(value))
        return len(value)


def _run_script_thread(name):
    global _script_running, _script_name
    try:
        path = _script_path(name)
        size = os.stat(path)[6]
        if size > _MAXIMUM_SCRIPT_BYTES:
            raise RuntimeError("Python scripts are limited to 65536 bytes")
        with open(path, "r") as script:
            source = script.read()
        namespace = {"__name__": "__main__", "__file__": path, "print": _captured_print}
        _append_output("\n--- run {} ---\n".format(name))
        exec(source, namespace, namespace)
        _append_output("--- finished {} ---\n".format(name))
    except Exception as error:
        _append_output("--- failed {} ---\n".format(name))
        sys.print_exception(error, _OutputWriter())
    finally:
        with _state_lock:
            _script_running = False
            _script_name = ""
        gc.collect()


def _start_script(name):
    global _script_running, _script_name
    _script_path(name)
    with _state_lock:
        if _script_running:
            raise RuntimeError("A Python script is already running")
        _script_running = True
        _script_name = name
    _thread.start_new_thread(_run_script_thread, (name,))


def _url_decode(value):
    output = bytearray()
    index = 0
    encoded = value.encode()
    while index < len(encoded):
        current = encoded[index]
        if current == 37 and index + 2 < len(encoded):
            output.append(int(encoded[index + 1:index + 3], 16))
            index += 3
        elif current == 43:
            output.append(32)
            index += 1
        else:
            output.append(current)
            index += 1
    return output.decode()


def _query_value(target, key):
    separator = target.find("?")
    if separator < 0:
        return ""
    for field in target[separator + 1:].split("&"):
        parts = field.split("=", 1)
        if len(parts) == 2 and parts[0] == key:
            return _url_decode(parts[1])
    return ""


def _constant_time_equals(left, right):
    difference = len(left) ^ len(right)
    maximum = max(len(left), len(right))
    for index in range(maximum):
        left_value = ord(left[index]) if index < len(left) else 0
        right_value = ord(right[index]) if index < len(right) else 0
        difference |= left_value ^ right_value
    return difference == 0


def _read_request(connection):
    data = bytearray()
    header_end = -1
    content_length = 0
    while len(data) <= _HTTP_LIMIT_BYTES:
        block = connection.recv(1024)
        if not block:
            break
        data.extend(block)
        if header_end < 0:
            header_end = data.find(b"\r\n\r\n")
            if header_end >= 0:
                for line in bytes(data[:header_end]).split(b"\r\n")[1:]:
                    if line.lower().startswith(b"content-length:"):
                        content_length = int(line.split(b":", 1)[1].strip())
                if content_length > _MAXIMUM_SCRIPT_BYTES + 1024:
                    raise ValueError("HTTP request body exceeds the Python workspace limit")
        if header_end >= 0 and len(data) >= header_end + 4 + content_length:
            break
    if header_end < 0 or len(data) < header_end + 4 + content_length:
        raise ValueError("Incomplete HTTP request")
    lines = bytes(data[:header_end]).decode().split("\r\n")
    request_line = lines[0].split(" ")
    if len(request_line) != 3:
        raise ValueError("Malformed HTTP request line")
    headers = {}
    for line in lines[1:]:
        parts = line.split(":", 1)
        if len(parts) == 2:
            headers[parts[0].strip().lower()] = parts[1].strip()
    body = bytes(data[header_end + 4:header_end + 4 + content_length])
    return request_line[0], request_line[1], headers, body


def _send(connection, status, content_type, body, extra_headers):
    encoded = body if isinstance(body, bytes) else body.encode()
    response = "HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\nCache-Control: no-store\r\n".format(
        status, content_type, len(encoded)
    )
    for key, value in extra_headers.items():
        response += "{}: {}\r\n".format(key, value)
    connection.send(response.encode() + b"\r\n" + encoded)


def _json_response(connection, status, value):
    _send(connection, status, "application/json; charset=utf-8", json.dumps(value), {})


def _cookie_token(headers):
    for field in headers.get("cookie", "").split(";"):
        parts = field.strip().split("=", 1)
        if len(parts) == 2 and parts[0] == "cardmind_py":
            return parts[1]
    return ""


def _authorized(headers):
    global _session_seen_at
    token = _cookie_token(headers)
    now = time.time()
    if token != _session_token or now - _session_seen_at > _SESSION_SECONDS:
        return False
    _session_seen_at = now
    return True


def _begin_session(connection):
    global _session_seen_at
    _session_seen_at = time.time()
    _send(connection, "303 See Other", "text/plain", "", {
        "Location": "/",
        "Set-Cookie": "cardmind_py={}; HttpOnly; SameSite=Strict; Path=/".format(_session_token),
    })


def _login_page(error):
    message = "<p class='error'>{}</p>".format(error) if error else ""
    template = """<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>CardMind Python</title><style>body{margin:0;background:#0a0c12;color:#edf1ff;font:15px system-ui;display:grid;place-items:center;min-height:100vh}.box{width:min(360px,calc(100% - 32px));background:#11141d;border:1px solid #2b3140;padding:24px}h1{margin-top:0}input,button{box-sizing:border-box;width:100%;padding:12px;margin-top:10px;background:#0d1412;color:#edf1ff;border:1px solid #34413d}button{background:#ff6b45;color:#111;font-weight:800}.error{color:#ff897f}</style><form class=box method=post action=/login><small>CARDMIND / PYTHON</small><h1>Python workspace</h1><p>Use the CardMind installation password.</p>__ERROR__<input name=password type=password required autocomplete=current-password><button>Open workspace</button></form>"""
    return template.replace("__ERROR__", message)


def _console_page():
    return """<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>CardMind Python</title><style>:root{color-scheme:dark;--bg:#0a0c12;--panel:#11141d;--line:#2b3140;--text:#edf1ff;--muted:#8d96aa;--accent:#ff6b45;--mint:#61e6b5;--side-width:260px;--output-height:230px}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui}.app{display:grid;grid-template-columns:var(--side-width) 7px minmax(0,1fr);height:100vh;min-height:560px;overflow:hidden}.side{padding:20px;overflow:auto}.main{padding:22px;display:grid;gap:10px;grid-template-rows:auto minmax(160px,1fr) 7px var(--output-height);min-width:0;min-height:0}h1,h2{margin:0 0 8px}small,p{color:var(--muted)}select,textarea,button,input{width:100%;background:#0d1412;color:var(--text);border:1px solid var(--line);padding:10px}button{font-weight:750;cursor:pointer}.primary{background:var(--accent);color:#111}.mint{border-color:#286552;color:var(--mint)}.row{display:flex;gap:8px}.row>*{flex:1}textarea{height:100%;min-height:0;resize:none;font:13px ui-monospace,monospace}.console-pane{display:grid;grid-template-rows:auto minmax(0,1fr);min-height:0}.output{white-space:pre-wrap;min-height:0;margin:0;overflow:auto;background:#05070b;border:1px solid var(--line);padding:12px;font:12px ui-monospace,monospace;overscroll-behavior:contain}.status{color:var(--mint)}.splitter{background:#151a22;position:relative;touch-action:none}.splitter::after{content:'';position:absolute;background:#485264;border-radius:4px}.splitter:hover::after,.splitter:focus-visible::after{background:var(--mint)}.splitter-side{cursor:col-resize}.splitter-side::after{width:2px;height:44px;left:2px;top:calc(50% - 22px)}.splitter-output{cursor:row-resize}.splitter-output::after{height:2px;width:44px;left:calc(50% - 22px);top:2px}.handoff{min-height:100vh;display:grid;place-items:center;padding:18px}.handoff-card{width:min(430px,100%);padding:24px;background:var(--panel);border:1px solid var(--line)}.handoff-card button{margin-top:12px}.handoff-card button:disabled{opacity:.5}@media(max-width:720px){.app{display:block;height:auto;min-height:100vh;overflow:visible}.side{border-bottom:1px solid var(--line)}.splitter-side{display:none}.main{height:calc(100vh - 250px);min-height:620px;padding:14px;grid-template-rows:auto minmax(260px,1fr) 7px var(--output-height)}}</style><div class=app><aside class=side><small>CARDMIND / PYTHON</small><h1>Python workspace</h1><p>Scripts share the CardMind microSD workspace.</p><select id=files size=10></select><div class=row><button id=newFile>New</button><button id=loadFile>Open</button></div><button id=back class=mint>Return to CardMind</button><button id=restart>Restart Python</button></aside><div class='splitter splitter-side' id=sideSplitter role=separator aria-label='Resize file panel' tabindex=0></div><main class=main><header><h2 id=title>No script selected</h2><span class=status id=status>Ready</span></header><textarea id=source spellcheck=false placeholder='# Write a MicroPython script'></textarea><div class='splitter splitter-output' id=outputSplitter role=separator aria-label='Resize Python output' tabindex=0></div><section class=console-pane><div class=row><button id=save>Save file</button><button id=run class=primary>Run</button><button id=refresh>Refresh output</button></div><pre class=output id=output tabindex=0></pre></section></main></div><script>const q=s=>document.querySelector(s);const root=document.documentElement;let current='',leaving=false;async function api(path,options){const r=await fetch(path,options);const v=await r.json();if(!r.ok)throw Error(v.error||('HTTP '+r.status));return v}function message(v,bad){q('#status').textContent=v;q('#status').style.color=bad?'#ff897f':'#61e6b5'}function clamp(value,minimum,maximum){return Math.min(maximum,Math.max(minimum,value))}function setSideWidth(value){const width=clamp(value,190,480);root.style.setProperty('--side-width',width+'px');localStorage.setItem('cardmind_python_side_width',String(width))}function setOutputHeight(value){const height=clamp(value,140,Math.floor(innerHeight*.62));root.style.setProperty('--output-height',height+'px');localStorage.setItem('cardmind_python_output_height',String(height))}function bindSideSplitter(){const splitter=q('#sideSplitter');splitter.onpointerdown=event=>{splitter.setPointerCapture(event.pointerId);splitter.onpointermove=move=>setSideWidth(move.clientX);splitter.onpointerup=()=>splitter.onpointermove=null}}function bindOutputSplitter(){const splitter=q('#outputSplitter');splitter.onpointerdown=event=>{splitter.setPointerCapture(event.pointerId);splitter.onpointermove=move=>setOutputHeight(innerHeight-move.clientY-22);splitter.onpointerup=()=>splitter.onpointermove=null}}function restoreLayout(){const side=Number(localStorage.getItem('cardmind_python_side_width'));const output=Number(localStorage.getItem('cardmind_python_output_height'));if(Number.isFinite(side)&&side>0)setSideWidth(side);if(Number.isFinite(output)&&output>0)setOutputHeight(output)}function updateOutput(value){const output=q('#output');const pinned=output.scrollHeight-output.scrollTop-output.clientHeight<28;if(output.textContent!==value)output.textContent=value;if(pinned)output.scrollTop=output.scrollHeight}async function state(){if(leaving)return;try{const v=await api('/api/state');const selected=q('#files').value;q('#files').innerHTML=v.files.map(f=>`<option value="${f.name}">${f.name} · ${f.bytes} B</option>`).join('');if(selected)q('#files').value=selected;updateOutput(v.output);q('#run').disabled=v.running;message(v.running?'Running '+v.script:'Ready',false)}catch(e){message(e.message,true)}}q('#loadFile').onclick=async()=>{try{current=q('#files').value;if(!current)throw Error('Select a script');const v=await api('/api/file?name='+encodeURIComponent(current));q('#source').value=v.content;q('#title').textContent=current;message('Loaded',false)}catch(e){message(e.message,true)}};q('#newFile').onclick=()=>{const n=prompt('Script filename','script.py');if(n){current=n;q('#title').textContent=n;q('#source').value='';message('New unsaved script',false)}};q('#save').onclick=async()=>{try{if(!current)throw Error('Create or open a script first');await api('/api/file?name='+encodeURIComponent(current),{method:'POST',headers:{'Content-Type':'text/plain;charset=utf-8'},body:q('#source').value});message('Saved',false);await state()}catch(e){message(e.message,true)}};q('#run').onclick=async()=>{try{if(!current)throw Error('Create or open a script first');await api('/api/run?name='+encodeURIComponent(current),{method:'POST'});message('Started',false);setTimeout(state,250)}catch(e){message(e.message,true)}};function showHandoff(){document.body.innerHTML=`<main class=handoff><section class=handoff-card><small>CARDMIND / PYTHON</small><h1>Returning to CardMind</h1><p id=handoffStatus>Waiting for the main firmware to start…</p><button id=openCardMind class=primary disabled>Open CardMind WebUI</button></section></main>`;const button=q('#openCardMind'),status=q('#handoffStatus');button.onclick=()=>location.replace('/');const probe=async()=>{try{const response=await fetch('/api/session',{cache:'no-store'});if(response.status===401||response.ok){status.textContent='CardMind is ready.';button.disabled=false;button.focus();return}}catch(e){}setTimeout(probe,750)};setTimeout(probe,1200)}q('#refresh').onclick=state;q('#back').onclick=async()=>{if(!confirm('Return to CardMind firmware?'))return;leaving=true;q('#back').disabled=true;message('CardMind is restarting…',false);try{await api('/api/cardmind',{method:'POST'})}catch(e){}showHandoff()};q('#restart').onclick=async()=>{if(confirm('Restart Python mode?'))await api('/api/restart',{method:'POST'})};restoreLayout();bindSideSplitter();bindOutputSplitter();state();setInterval(state,2000)</script>"""


def _handle_api(connection, method, target, headers, body):
    if not _authorized(headers):
        _json_response(connection, "401 Unauthorized", {"error": "Session expired"})
        return
    path = target.split("?", 1)[0]
    if method == "GET" and path == "/api/state":
        with _state_lock:
            value = {
                "files": _list_scripts(),
                "running": _script_running,
                "script": _script_name,
                "output": _script_output,
            }
        _json_response(connection, "200 OK", value)
    elif method == "GET" and path == "/api/file":
        name = _query_value(target, "name")
        path_value = _script_path(name)
        size = os.stat(path_value)[6]
        if size > _MAXIMUM_SCRIPT_BYTES:
            raise ValueError("Python scripts are limited to 65536 bytes")
        with open(path_value, "r") as script:
            content = script.read()
        _json_response(connection, "200 OK", {"name": name, "content": content})
    elif method == "POST" and path == "/api/file":
        name = _query_value(target, "name")
        path_value = _script_path(name)
        body.decode("utf-8")
        temporary = path_value + ".tmp"
        with open(temporary, "wb") as output:
            output.write(body)
        try:
            os.remove(path_value)
        except OSError as error:
            if error.args[0] != 2:
                raise
        os.rename(temporary, path_value)
        _json_response(connection, "200 OK", {"ok": True})
    elif method == "POST" and path == "/api/run":
        name = _query_value(target, "name")
        _start_script(name)
        _json_response(connection, "202 Accepted", {"ok": True})
    elif method == "POST" and path == "/api/cardmind":
        _json_response(connection, "202 Accepted", {"ok": True})
        time.sleep_ms(150)
        _cardmind_partition().set_boot()
        machine.reset()
    elif method == "POST" and path == "/api/restart":
        _json_response(connection, "202 Accepted", {"ok": True})
        time.sleep_ms(150)
        machine.reset()
    else:
        _json_response(connection, "404 Not Found", {"error": "Unknown Python workspace endpoint"})


def _serve(password, address, namespace):
    global _session_token, _session_seen_at
    _session_token = binascii.hexlify(machine.unique_id() + os.urandom(12)).decode()
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("0.0.0.0", 80))
    listener.listen(3)
    print("PYTHON_CONSOLE ready=http://{}/".format(address))
    while True:
        connection, _ = listener.accept()
        connection.settimeout(5)
        try:
            method, target, headers, body = _read_request(connection)
            path = target.split("?", 1)[0]
            if method == "GET" and path == "/handoff":
                supplied = _query_value(target, "token")
                expected = _try_read_blob(namespace, "handoff", 33)
                if not expected or not _constant_time_equals(supplied, expected):
                    _send(connection, "403 Forbidden", "text/html; charset=utf-8", _login_page("Handoff expired"), {})
                else:
                    _clear_key(namespace, "handoff")
                    namespace.commit()
                    _begin_session(connection)
            elif method == "POST" and path == "/login":
                fields = {}
                for field in body.decode().split("&"):
                    parts = field.split("=", 1)
                    if len(parts) == 2:
                        fields[parts[0]] = _url_decode(parts[1])
                if fields.get("password", "") != password:
                    _send(connection, "403 Forbidden", "text/html; charset=utf-8", _login_page("Incorrect password"), {})
                else:
                    _begin_session(connection)
            elif method == "GET" and path == "/":
                page = _console_page() if _authorized(headers) else _login_page("")
                _send(connection, "200 OK", "text/html; charset=utf-8", page, {})
            elif path.startswith("/api/"):
                _handle_api(connection, method, target, headers, body)
            else:
                _send(connection, "404 Not Found", "text/plain; charset=utf-8", "Not found", {})
        except (OSError, ValueError, RuntimeError) as error:
            try:
                _json_response(connection, "400 Bad Request", {"error": str(error)})
            except OSError:
                pass
        finally:
            connection.close()
        gc.collect()


def start():
    esp32.Partition.mark_app_valid_cancel_rollback()
    namespace = esp32.NVS(_CONFIG_NAMESPACE)
    _clear_key(namespace, "mode_error")
    namespace.commit()
    _mount_sd()
    try:
        if _apply_pending_update(namespace):
            return
    except (OSError, ValueError, RuntimeError) as error:
        _remember_update_error(namespace, error)
    password = _read_blob(namespace, "console_pass", 97)
    if len(password) < 8:
        raise RuntimeError("Python mode has no synchronized console password")
    address = _connect_wifi(namespace)
    _serve(password, address, namespace)
