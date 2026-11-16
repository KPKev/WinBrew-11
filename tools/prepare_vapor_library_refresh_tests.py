"""Expose upstream entitlement fixtures without importing its test entry point."""
from pathlib import Path
import sys

if __name__ == "__main__":
    source, output = map(Path, sys.argv[1:])
    text = source.read_text(encoding="utf-8")
    marker = "\nint main() {"
    if text.count(marker) != 1:
        raise SystemExit("Upstream entitlement test entry point changed")
    text = text.replace("{{80, 90, 95}}", "{{11, 80, 90, 95}}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text.split(marker)[0] + "\n", encoding="utf-8")
