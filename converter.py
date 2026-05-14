from __future__ import annotations

import argparse
from pathlib import Path


def find_sys_file(path_arg: str | None) -> Path:
    if path_arg:
        candidate = Path(path_arg).expanduser().resolve()
        if candidate.is_file():
            return candidate
        raise FileNotFoundError(f"File not found: {candidate}")

    # Default: search for the first .sys under the build folder
    base_dir = Path.cwd()
    build_dir = base_dir / "build"
    if build_dir.is_dir():
        matches = sorted(build_dir.rglob("*.sys"))
        if matches:
            return matches[0]

    raise FileNotFoundError(
        "Could not find a .sys file. Pass a path with --sys or run from the repo root."
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a .sys file to a C++ byte vector"
    )
    parser.add_argument(
        "--sys",
        dest="sys_path",
        help="Path to the .sys file (optional if build/ has one)"
    )
    args = parser.parse_args()

    sys_file = find_sys_file(args.sys_path)
    with sys_file.open("rb") as f:
        data = f.read()

    print("static std::vector<uint8_t> image = {")
    for i, b in enumerate(data):
        if i % 16 == 0:
            print("\n    ", end="")
        print(f"0x{b:02X}, ", end="")
    print("\n};")


if __name__ == "__main__":
    main()