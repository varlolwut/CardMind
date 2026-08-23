import ast
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
    start = load_function_node(source_path, "start")
    first_statement = ast.unparse(start.body[0])
    if first_statement != "esp32.Partition.mark_app_valid_cancel_rollback()":
        raise RuntimeError("MicroPython supervisor must confirm the OTA image before startup")
    print("MICROPYTHON_SUPERVISOR_TEST result=pass")


if __name__ == "__main__":
    main()
