#!/usr/bin/env python3
"""Build the exact console-tested cleanup helper and emit its embedded bytes."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: build-vapor-game-cleaner.py OUTPUT_CPP")
sdk = Path(os.environ.get("PS5_PAYLOAD_SDK", root / "vendor/playden-source/deps/ps5-payload-sdk"))
binary = root / "build/vapor-game-cleaner-pinned.elf"
binary.parent.mkdir(parents=True, exist_ok=True)
# The payload tested on the console was built with the SDK's default LLVM 21.
# The enclosing native-app build pins LLVM 18 for a different target; do not
# inherit that override for this separately pinned ELF.
environment = os.environ.copy()
environment.pop("LLVM_CONFIG", None)
subprocess.run([str(sdk / "bin/prospero-clang++"), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O2",
                "-Wl,--build-id=none", "-Isrc", "-Itools/vapor-game-cleaner", "tools/vapor-game-cleaner/main.cpp",
                "-o", str(binary)], cwd=root, env=environment, check=True)
data = binary.read_bytes()
expected = "0de8cbe5441aba99859af79857c50a8fad897c0e4cfee5511b1654266aaa3ea2"
if not data.startswith(b"\x7fELF") or hashlib.sha256(data).hexdigest() != expected:
    raise SystemExit("Cleanup helper differs from the console-tested payload")
lines = [",".join(f"0x{x:02x}" for x in data[i:i + 24]) for i in range(0, len(data), 24)]
source = ('#include <cstddef>\nextern "C" {\nextern const unsigned char vapor_game_cleaner[] = {\n' +
          ',\n'.join(lines) + '\n};\nextern const std::size_t vapor_game_cleaner_size = sizeof(vapor_game_cleaner);\n}\n')
Path(sys.argv[1]).write_text(source, encoding="utf-8", newline="\n")
print(f"PASS: embedded tested cleanup helper {expected}")
