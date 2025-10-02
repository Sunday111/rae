#!/usr/bin/env python3

from pathlib import Path
import shutil

SCRIPT_DIR = Path(__file__).parent.resolve()
WORKSPACE_DIR = SCRIPT_DIR.parent
VSCODE_SETTINGS_SRC = WORKSPACE_DIR / "vscode_settings"
VSCODE_SETTINGS_DST = WORKSPACE_DIR / ".vscode"


def main():
    shutil.rmtree(VSCODE_SETTINGS_DST, ignore_errors=True)
    shutil.copytree(src=VSCODE_SETTINGS_SRC, dst=VSCODE_SETTINGS_DST)

    for path in VSCODE_SETTINGS_DST.rglob("*.json"):
        content: str = ""
        with open(file=path, mode="rt", encoding="utf-8") as file:
            content = file.read()

        content = content.replace("${workspaceFolder}", WORKSPACE_DIR.as_posix())

        with open(file=path, mode="wt", encoding="utf-8") as file:
            file.write(content)


if __name__ == "__main__":
    main()
