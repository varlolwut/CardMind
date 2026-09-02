import ast
import builtins
import io
import sys
import traceback
import types
from pathlib import Path


def load_function(source_path: Path, function_name: str):
    module = ast.parse(source_path.read_text(encoding="utf-8"), filename=str(source_path))
    matches = [
        node
        for node in module.body
        if isinstance(node, ast.FunctionDef) and node.name == function_name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one {function_name} function")
    namespace = {}
    exec(compile(ast.Module(body=matches, type_ignores=[]), str(source_path), "exec"), namespace)
    return namespace[function_name]


def load_function_node(source_path: Path, function_name: str) -> ast.FunctionDef:
    module = ast.parse(source_path.read_text(encoding="utf-8"), filename=str(source_path))
    matches = [
        node
        for node in module.body
        if isinstance(node, ast.FunctionDef) and node.name == function_name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one {function_name} function")
    return matches[0]


def main() -> None:
    source_path = Path("micropython/vfs/cardmind_supervisor.py")
    login_page = load_function(source_path, "_login_page")
    page = login_page("Incorrect password")
    if "body{margin:0" not in page:
        raise RuntimeError("MicroPython login page lost its CSS declarations")
    if "<p class='error'>Incorrect password</p>" not in page:
        raise RuntimeError("MicroPython login page did not render its error message")
    if "__ERROR__" in page:
        raise RuntimeError("MicroPython login page retained its template marker")
    constant_time_equals = load_function(source_path, "_constant_time_equals")
    if not constant_time_equals("A1b2", "A1b2"):
        raise RuntimeError("MicroPython handoff token comparison rejected an exact match")
    if constant_time_equals("A1b2", "A1b3") or constant_time_equals("A1b2", "A1b20"):
        raise RuntimeError("MicroPython handoff token comparison accepted a mismatch")
    send_response = load_function(source_path, "_send")

    class PartialConnection:
        def __init__(self) -> None:
            self.payload = bytearray()

        def send(self, value: bytes) -> int:
            chunk = value[:7]
            self.payload.extend(chunk)
            return len(chunk)

    partial_connection = PartialConnection()
    send_response(partial_connection, "200 OK", "text/plain", "ready", {})
    if not bytes(partial_connection.payload).endswith(b"\r\n\r\nready"):
        raise RuntimeError("MicroPython HTTP writer lost a partial socket write")
    safe_script_name = load_function(source_path, "_safe_script_name")
    if not safe_script_name("hello_world-2.py"):
        raise RuntimeError("MicroPython script name validation rejected an ASCII filename")
    if safe_script_name("../escape.py") or safe_script_name("кириллица.py"):
        raise RuntimeError("MicroPython script name validation accepted an unsafe filename")
    safe_one_shot_path = load_function(source_path, "_safe_one_shot_path")
    if not safe_one_shot_path("project/tools/check.py"):
        raise RuntimeError("One-shot path validation rejected a normalized nested .py path")
    if safe_one_shot_path("../escape.py") or safe_one_shot_path("check.PY"):
        raise RuntimeError("One-shot path validation accepted traversal or non-exact extension")

    decode_u64 = load_function(source_path, "_decode_u64")
    valid_hex = load_function(source_path, "_valid_hex")
    parse_run_blob = load_function(source_path, "_parse_run_blob")
    parse_run_blob.__globals__.update({
        "_RUN_BLOB_BYTES": 91,
        "_RUN_VERSION": 1,
        "_RUN_PENDING": 1,
        "_RUN_CLAIMED": 2,
        "_RUN_COMPLETE": 3,
        "_valid_hex": valid_hex,
        "_decode_u64": decode_u64,
    })
    run_blob = bytearray(91)
    run_blob[0:3] = bytes((1, 1, 1))
    run_blob[3:19] = b"0123456789abcdef"
    run_blob[19:51] = b"r" * 32
    run_blob[83:91] = (42).to_bytes(8, "little")
    parsed_blob = parse_run_blob(run_blob)
    if parsed_blob["pending_id"] != "0123456789abcdef" or parsed_blob["audit_sequence"] != 42:
        raise RuntimeError("One-shot 91-byte state lost exact identity or uint64 sequence")
    invalid_blob = bytearray(run_blob)
    invalid_blob[51] = 1
    try:
        parse_run_blob(invalid_blob)
    except ValueError:
        pass
    else:
        raise RuntimeError("Pending one-shot state accepted a premature result hash")
    source = source_path.read_text(encoding="utf-8")
    if 'path == "/handoff"' not in source or 'namespace.erase_key(key)' not in source:
        raise RuntimeError("MicroPython one-time browser handoff is missing")
    if "machine.Pin(0" in source:
        raise RuntimeError("MicroPython must not treat the ADV G0 signal as a runtime button")
    for required in (
        "id=sideSplitter",
        "id=outputSplitter",
        "cardmind_python_side_width",
        "cardmind_python_output_height",
        "output.scrollTop=output.scrollHeight",
        "Try opening CardMind",
        "button.hidden=false,8000",
        "fetch('/api/session?return=python",
        "new AbortController()",
        "controller.abort(),1200",
        "location.replace('/?return=python&time='+Date.now())",
        'namespace.set_i32("open_web", 1)',
        'with open(_CARDMIND_HANDOFF_PATH, "w") as marker:',
        "os.sync()",
        "namespace.commit()",
    ):
        if required not in source:
            raise RuntimeError("MicroPython browser workspace is missing " + required)
    start = load_function_node(source_path, "start")
    first_statement = ast.unparse(start.body[0])
    if first_statement != "esp32.Partition.mark_app_valid_cancel_rollback()":
        raise RuntimeError("MicroPython supervisor must confirm the OTA image before startup")
    start_source = ast.unparse(start)
    if not (
        start_source.index("_start_one_shot_if_present(namespace)")
        < start_source.index("_clear_key(namespace,")
        < start_source.index("namespace.commit()")
        < start_source.index("_mount_sd()")
        < start_source.index("_connect_wifi(namespace)")
    ):
        raise RuntimeError("One-shot inspection changed the baseline manual startup order")
    execute_source = ast.unparse(load_function_node(source_path, "_execute_one_shot"))
    if not (
        execute_source.index("_cardmind_partition().set_boot()")
        < execute_source.index("_persist_run_blob(namespace, run_blob)")
        < execute_source.index("machine.WDT(0, timeout=_RUN_WDT_MS)")
        < execute_source.index("_mount_sd()")
        < execute_source.index("_strict_request(blob)")
        < execute_source.index("exec(source, scope, scope)")
        < execute_source.index("_persist_result(namespace")
    ):
        raise RuntimeError("One-shot claim, watchdog, SD, exec, or result ordering changed")
    for forbidden in ("sys.stdout =", "sys.stderr ="):
        if forbidden in execute_source:
            raise RuntimeError("One-shot executor mutates a fixed MicroPython stream")
    for required in (
        "builtins.print = print_adapter",
        "del builtins.print",
        "sys.print_exception(error, stderr)",
    ):
        if required not in execute_source:
            raise RuntimeError("One-shot native output routing is missing " + required)

    class StopAtMount(Exception):
        pass

    execution_events = []

    class TestPartition:
        def set_boot(self) -> None:
            execution_events.append("boot")

    class TestMachine:
        @staticmethod
        def WDT(identifier: int, timeout: int):
            if identifier != 0 or timeout != 30000:
                raise RuntimeError("One-shot watchdog contract changed")
            execution_events.append("watchdog")
            return object()

        @staticmethod
        def reset() -> None:
            execution_events.append("reset")

    def persist_claim(_namespace, value) -> None:
        if value[1] != 2:
            raise RuntimeError("One-shot claim was not persisted as claimed")
        execution_events.append("claim")

    def stop_at_mount() -> None:
        execution_events.append("mount")
        raise StopAtMount()

    execute_one_shot = load_function(source_path, "_execute_one_shot")
    execute_one_shot.__globals__.update({
        "_RUN_CLAIMED": 2,
        "_RUN_WDT_MS": 30000,
        "_cardmind_partition": lambda: TestPartition(),
        "_persist_run_blob": persist_claim,
        "_mount_sd": stop_at_mount,
        "machine": TestMachine,
    })
    try:
        execute_one_shot(object(), bytearray(91), {})
    except StopAtMount:
        pass
    else:
        raise RuntimeError("One-shot ordering check did not reach the SD boundary")
    if execution_events != ["boot", "claim", "watchdog", "mount"]:
        raise RuntimeError("One-shot effects occurred in an unsafe order")

    runtime_names = {
        "_OneShotBudget",
        "_OneShotWriter",
        "_OneShotSys",
        "_make_one_shot_print",
        "_restore_module_entry",
        "_normalize_exit_status",
        "_execute_one_shot",
    }
    supervisor_module = ast.parse(source, filename=str(source_path))
    runtime_nodes = [
        node
        for node in supervisor_module.body
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)) and node.name in runtime_names
    ]
    if {node.name for node in runtime_nodes} != runtime_names:
        raise RuntimeError("One-shot runtime test could not load every production definition")
    runtime_globals = {"builtins": builtins, "io": io, "sys": sys}
    exec(
        compile(ast.Module(body=runtime_nodes, type_ignores=[]), str(source_path), "exec"),
        runtime_globals,
    )
    runtime_globals["_MAXIMUM_OUTPUT_BYTES"] = 7
    budget = runtime_globals["_OneShotBudget"]()
    stdout_writer = runtime_globals["_OneShotWriter"](budget)
    stderr_writer = runtime_globals["_OneShotWriter"](budget)
    if stdout_writer.write("A€") != 2:
        raise RuntimeError("One-shot direct text write returned a byte count")
    native_chunk = bytearray("€X".encode("utf-8"))
    if stderr_writer.write(native_chunk) != len(native_chunk):
        raise RuntimeError("One-shot native bytearray write returned a character count")
    if stdout_writer.value() != "A€" or stderr_writer.value() != "€":
        raise RuntimeError("One-shot shared budget split a UTF-8 character")
    if stdout_writer.truncated or not stderr_writer.truncated or budget.remaining != 0:
        raise RuntimeError("One-shot shared budget or per-stream truncation changed")
    try:
        stdout_writer.write(b"unsupported")
    except TypeError:
        pass
    else:
        raise RuntimeError("One-shot writer accepted a non-native byte input")
    runtime_globals["_MAXIMUM_OUTPUT_BYTES"] = 16384

    probe_name = "_cardmind_p5_output_probe"
    if probe_name in sys.modules:
        raise RuntimeError("One-shot imported-print probe name is not collision-free")
    probe = types.ModuleType(probe_name)
    exec("def emit():\n    print('IMPORTED')\n", probe.__dict__)
    sys.modules[probe_name] = probe

    def run_one_shot(script: str) -> tuple[int, str, str, list[str]]:
        events: list[str] = []
        captured: list[tuple[int, str, str]] = []
        native_print = builtins.print
        sys_existed = "sys" in sys.modules
        original_sys = sys.modules.get("sys")
        usys_existed = "usys" in sys.modules
        original_usys = sys.modules.get("usys")
        print_exception_existed = hasattr(sys, "print_exception")
        original_print_exception = getattr(sys, "print_exception", None)

        class RuntimePartition:
            def set_boot(self) -> None:
                events.append("boot")

        class RuntimeMachine:
            @staticmethod
            def WDT(identifier: int, timeout: int):
                if identifier != 0 or timeout != 30000:
                    raise RuntimeError("One-shot runtime watchdog contract changed")
                events.append("watchdog")
                return object()

            @staticmethod
            def reset() -> None:
                events.append("reset")

        class RuntimeTime:
            @staticmethod
            def sleep_ms(value: int) -> None:
                if value != 100:
                    raise RuntimeError("One-shot completion delay changed")
                events.append("sleep")

        def persist_claim(_namespace, value: bytearray) -> None:
            if value[1] != 2:
                raise RuntimeError("One-shot runtime claim did not precede execution")
            events.append("claim")

        def mount_sd() -> None:
            events.append("mount")

        def strict_request(_blob) -> tuple[str, str]:
            events.append("strict")
            return "/sd/assistant/files/project/run.py", script

        def persist_result(_namespace, _run_blob, _blob, exit_status, stdout, stderr) -> None:
            if hasattr(builtins, "print"):
                raise RuntimeError("One-shot builtin override remained before persistence")
            if ("sys" in sys.modules) != sys_existed or \
                    (sys_existed and sys.modules["sys"] is not original_sys):
                raise RuntimeError("One-shot sys module entry was not restored before persistence")
            if ("usys" in sys.modules) != usys_existed or \
                    (usys_existed and sys.modules["usys"] is not original_usys):
                raise RuntimeError("One-shot usys module entry was not restored before persistence")
            captured.append((exit_status, stdout.value(), stderr.value()))
            events.append("persist")

        def host_print_exception(error: BaseException, stream: io.IOBase) -> None:
            traceback.print_exception(type(error), error, error.__traceback__, file=stream)

        def restore_module(name: str, existed: bool, value) -> None:
            if existed:
                sys.modules[name] = value
            elif name in sys.modules:
                del sys.modules[name]

        runtime_globals.update({
            "_RUN_CLAIMED": 2,
            "_RUN_WDT_MS": 30000,
            "_cardmind_partition": lambda: RuntimePartition(),
            "_persist_run_blob": persist_claim,
            "_mount_sd": mount_sd,
            "_strict_request": strict_request,
            "_persist_result": persist_result,
            "machine": RuntimeMachine,
            "time": RuntimeTime,
            "print": lambda *values, **options: None,
        })
        try:
            sys.print_exception = host_print_exception
            runtime_globals["_execute_one_shot"](
                object(), bytearray(91), {"pending_id": "0123456789abcdef"}
            )
        finally:
            builtins.print = native_print
            restore_module("sys", sys_existed, original_sys)
            restore_module("usys", usys_existed, original_usys)
            if print_exception_existed:
                sys.print_exception = original_print_exception
            else:
                del sys.print_exception
        if len(captured) != 1:
            raise RuntimeError("One-shot runtime did not persist exactly one result")
        if events != [
            "boot", "claim", "watchdog", "mount", "strict", "persist", "sleep", "reset"
        ]:
            raise RuntimeError("One-shot runtime effect order or reset count changed")
        return captured[0][0], captured[0][1], captured[0][2], events

    try:
        success = run_one_shot(
            "import sys\n"
            "import usys\n"
            "import _cardmind_p5_output_probe\n"
            "print('OUT')\n"
            "print('ERR', file=sys.stderr)\n"
            "sys.stdout.write('DIRECT-SYS\\n')\n"
            "usys.stderr.write('DIRECT-USYS\\n')\n"
            "_cardmind_p5_output_probe.emit()\n"
        )
        if success[0] != 0 or success[1] != "OUT\nDIRECT-SYS\nIMPORTED\n":
            raise RuntimeError("One-shot runtime lost bare, imported, or direct stdout")
        if success[2] != "ERR\nDIRECT-USYS\n":
            raise RuntimeError("One-shot runtime lost separate stderr")
        failed = run_one_shot("print('BEFORE')\nraise ValueError('boom')\n")
        if failed[0] != 1 or failed[1] != "BEFORE\n":
            raise RuntimeError("One-shot exception path lost stdout or exit status")
        if "ValueError" not in failed[2] or "boom" not in failed[2]:
            raise RuntimeError("One-shot native-shaped traceback was not captured")
        exited = run_one_shot("print('EXIT')\nraise SystemExit(7)\n")
        if exited[0] != 7 or exited[1] != "EXIT\n" or exited[2]:
            raise RuntimeError("One-shot SystemExit path changed output or exit status")
    finally:
        if probe_name in sys.modules:
            del sys.modules[probe_name]

    class ExitWriter:
        def __init__(self) -> None:
            self.output = ""

        def write(self, value: str) -> None:
            self.output += value

    normalize_exit_status = load_function(source_path, "_normalize_exit_status")
    for status in (-2147483648, 0, 2147483647):
        writer = ExitWriter()
        if normalize_exit_status(SystemExit(status), writer) != status or writer.output:
            raise RuntimeError("In-range SystemExit status changed during normalization")
    for status in (-2147483649, 2147483648, "invalid"):
        writer = ExitWriter()
        if normalize_exit_status(SystemExit(status), writer) != 1 or \
                "signed 32-bit" not in writer.output:
            raise RuntimeError("Out-of-range SystemExit was not normalized to status 1")

    class TestNamespace:
        def __init__(self, value):
            self.value = value

        def get_blob(self, key: str, target: bytearray) -> int:
            if key != "run":
                raise RuntimeError("One-shot lookup used an unexpected NVS key")
            if self.value is None:
                raise OSError(-4354)
            target[:len(self.value)] = self.value
            return len(self.value)

    start_one_shot = load_function(source_path, "_start_one_shot_if_present")
    start_one_shot.__globals__.update({
        "_RUN_BLOB_BYTES": 91,
        "_RUN_PENDING": 1,
        "_parse_run_blob": parse_run_blob,
        "_cardmind_partition": lambda: TestPartition(),
        "_execute_one_shot": lambda _namespace, _run_blob, _blob: execution_events.append("execute"),
        "machine": TestMachine,
    })
    execution_events.clear()
    if start_one_shot(TestNamespace(None)) or execution_events:
        raise RuntimeError("Missing one-shot state did not preserve manual startup")
    for invalid_state in (bytearray(1), bytearray(91), bytearray(run_blob[:1] + bytes((2,)) + run_blob[2:])):
        execution_events.clear()
        if not start_one_shot(TestNamespace(invalid_state)):
            raise RuntimeError("Invalid one-shot state continued into manual startup")
        if execution_events != ["boot", "reset"]:
            raise RuntimeError("Invalid one-shot state did not fail closed to CardMind")
    execution_events.clear()
    if not start_one_shot(TestNamespace(run_blob)) or execution_events != ["execute"]:
        raise RuntimeError("Valid pending one-shot state was not dispatched exactly once")
    persist_source = ast.unparse(load_function_node(source_path, "_persist_result"))
    for required in (
        "output.flush()",
        "os.sync()",
        "_verify_file_hash",
        "os.rename",
        "run_blob[1] = _RUN_COMPLETE",
        "_persist_run_blob(namespace, run_blob)",
    ):
        if required not in persist_source:
            raise RuntimeError("One-shot result durability is missing " + required)
    handle_api_source = ast.unparse(load_function_node(source_path, "_handle_api"))
    handle_api = load_function_node(source_path, "_handle_api")
    if [argument.arg for argument in handle_api.args.args][-1] != "namespace":
        raise RuntimeError("MicroPython API handler does not receive its NVS namespace")
    if "machine.reset()" in handle_api_source:
        raise RuntimeError("MicroPython API handler resets before its response is closed")
    serve_source = ast.unparse(load_function_node(source_path, "_serve"))
    if "_handle_api(connection, method, target, headers, body, namespace)" not in serve_source:
        raise RuntimeError("MicroPython server does not pass NVS to the API handler")
    if serve_source.index("connection.close()") > serve_source.index("machine.reset()"):
        raise RuntimeError("MicroPython handoff resets before closing its HTTP response")
_version_source_path = (
    __import__("pathlib").Path(__file__).resolve().parents[1]
    / "micropython"
    / "vfs"
    / "cardmind_supervisor.py"
)
_version_tree = ast.parse(_version_source_path.read_text(encoding="utf-8"))
_strict_request_node = next(
    node
    for node in _version_tree.body
    if isinstance(node, ast.FunctionDef) and node.name == "_strict_request"
)


def _reject_later_path_validation(path):
    raise AssertionError("Malformed request version reached path validation")


def _assert_malformed_version_rejected(version):
    json_module = __import__("json")
    request_bytes = json_module.dumps({
        "version": version,
        "pending_id": "0123456789abcdef",
        "path": "/projects/version.py",
        "size": 1,
        "sha256": "0" * 64,
        "audit_sequence": 1,
        "surface": "device",
    }).encode("utf-8")
    namespace = {
        "_read_bounded_bytes": lambda path, maximum_bytes: request_bytes,
        "_RUN_REQUEST_PATH": "/request.json",
        "_RUN_REQUEST_MAXIMUM_BYTES": 1024,
        "_sha256": lambda value: b"request-sha",
        "json": json_module,
        "_RUN_VERSION": 1,
        "_safe_one_shot_path": _reject_later_path_validation,
    }
    module = ast.Module(body=[_strict_request_node], type_ignores=[])
    exec(compile(module, str(_version_source_path), "exec"), namespace)
    blob = {
        "request_sha": b"request-sha",
        "surface": 1,
        "pending_id": "0123456789abcdef",
        "audit_sequence": 1,
    }
    try:
        namespace["_strict_request"](blob)
    except ValueError:
        return
    raise AssertionError("Malformed request version was accepted")


_assert_malformed_version_rejected(True)
_assert_malformed_version_rejected(1.0)

if __name__ == "__main__":
    main()
    print("MICROPYTHON_SUPERVISOR_TEST result=pass")
