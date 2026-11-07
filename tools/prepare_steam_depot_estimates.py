#!/usr/bin/env python3
"""Guard depot-size estimates in an isolated copy of the pinned host source."""
from pathlib import Path

OLD = '''  std::uint64_t install = 0;
  std::uint64_t download = 0;
  for (const auto& depot : depots) {
    install += depot.size;
    // Some appinfo reports a download larger than the install (FINAL FANTASY XIII, issue #5); compressed chunks never are.
    download += depot.size && depot.download_size > depot.size ? depot.size : depot.download_size;
  }
  if (install) details.install_bytes = install;
  if (download) details.download_bytes = download;'''
NEW = '''  std::uint64_t install = 0;
  std::uint64_t download = 0;
  bool install_known = true, download_known = true;
  for (const auto& depot : depots) {
    // Zero denotes missing metadata, so a partial sum is not a game total.
    if (!depot.size || depot.size > UINT64_MAX - install) install_known = false;
    else install += depot.size;
    // Keep upstream's cap for implausible compressed sizes when both exist.
    const auto compressed = depot.size && depot.download_size > depot.size ? depot.size : depot.download_size;
    if (!depot.download_size || compressed > UINT64_MAX - download) download_known = false;
    else download += compressed;
  }
  if (install_known && install) details.install_bytes = install;
  if (download_known && download) details.download_bytes = download;'''

def transform(source: str) -> str:
    if source.count(OLD) != 1 or source.count("namespace playden::steam {") != 1:
        raise ValueError("Unexpected depot estimate source; refusing ambiguous transformation")
    return source.replace(OLD, NEW, 1).replace(
        "namespace playden::steam {",
        'extern "C" int wine11_verified_depot_estimates() { return 1; }\n\nnamespace playden::steam {', 1)

def apply(path: Path) -> None:
    path.write_text(transform(path.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("Source and output must be different files")
    args.output.write_text(transform(args.source.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")
