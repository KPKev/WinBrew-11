#!/usr/bin/env python3
"""Build the runtime-checked owned-root daemon without fetching dependencies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parent.parent
SOURCES = ("owned_root_daemon.c", "donor_transaction.c",
           "vnode_ref_probe.c", "donor_transaction.h",
           "vnode_ref_probe.h", "lapy_elevation_protocol.h", "winbrew_shared_folders.h")
# Only rule builds compile and record these.
RULE_SOURCES = ("title_rule.c", "title_rule.h")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def valid_target_title(title):
    # Explicit native titles still use an exact runtime identity comparison.
    # The service wildcard retains its existing PPSA-only discovery scope.
    return title == "*" or re.fullmatch(r"(?:PPSA|FAKE)[0-9]{5}", title) is not None


def valid_title_rule(prefix, allow_list):
    # A rule names a range (four letters, up to four digits) and the root-owned
    # allow-list the helper reads; both are compiled in.
    return (re.fullmatch(r"[A-Z]{4}[0-9]{0,4}", prefix or "") is not None and
            re.fullmatch(r"(?:/[A-Za-z0-9._-]+)+", allow_list or "") is not None and
            len(allow_list) < 256 and
            not any(part in (".", "..") for part in allow_list.split("/")))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path,
                        default=os.environ.get("PS5_PAYLOAD_SDK"), required=False)
    parser.add_argument("--logging-client", type=Path, required=True)
    parser.add_argument("--service", action="store_true",
                        help="build a resident daemon for repeated cooperative requests")
    parser.add_argument("--elf-helper", action="store_true",
                        help="build a one-request helper for the local elfldr client")
    parser.add_argument("--title", help="Exact PPSA or FAKE title, or * for all PPSA titles")
    parser.add_argument("--title-rule", metavar="PREFIX",
                        help="helper serves titles PREFIX+digits listed in --allow-list")
    parser.add_argument("--allow-list", metavar="PATH",
                        help="root-owned allow-list a --title-rule helper reads")
    parser.add_argument("--require-client-result", action="store_true",
                        help="require the laboratory data read/write result file")
    parser.add_argument("--max-requests", type=int, default=0,
                        help="bound a service build for controlled tests; 0 is unlimited")
    args = parser.parse_args()
    if args.max_requests < 0 or args.max_requests > 1000000:
        parser.error("--max-requests must be between 0 and 1000000")
    if args.service and args.elf_helper:
        parser.error("--service and --elf-helper are separate build modes")
    if not args.service and args.max_requests:
        parser.error("--max-requests requires --service")
    rule = args.title_rule is not None or args.allow_list is not None
    if rule:
        if not args.elf_helper or args.title:
            parser.error("--title-rule requires --elf-helper and replaces --title")
        if not valid_title_rule(args.title_rule, args.allow_list):
            parser.error("--title-rule must be four letters and up to four digits, "
                         "and --allow-list an absolute path")
    title = (args.title_rule + "*" if rule else
             args.title or ("*" if args.service else "PPSA99994"))
    if not rule and not valid_target_title(title):
        parser.error("--title must be an exact PPSA or FAKE title, or *")
    if not args.service and title == "*":
        parser.error("one-request build requires an exact title")
    require_client_result = args.require_client_result or (
        not args.service and not args.elf_helper)
    if args.sdk is None:
        parser.error("--sdk or PS5_PAYLOAD_SDK required")
    sdk = args.sdk.resolve()
    logging = args.logging_client.resolve()
    sources = SOURCES + (RULE_SOURCES if rule else ())
    paths = [ROOT / "source" / name for name in sources]
    paths += [Path(__file__).resolve(), logging / "ps5log.h"]
    if not all(path.is_file() for path in paths):
        raise FileNotFoundError("candidate source or logging client missing")
    # Key inputs by location-independent names so the identity, and the ELF that embeds it, are
    # the same in every checkout.
    names = [f"source/{name}" for name in sources] + ["tools/build_owned_daemon.py", "logging/ps5log.h"]
    inputs = {name: digest(path) for name, path in zip(names, paths)}
    sdk_inputs = {str(path.relative_to(sdk)): digest(path)
                  for path in sorted((sdk / "target").rglob("*"))
                  if path.is_file()}
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             f'-DTARGET_TITLE="{title}"',
             f'-DLAPY_SERVICE={int(args.service)}',
             f'-DLAPY_ELF_HELPER={int(args.elf_helper)}',
             f'-DLAPY_REQUIRE_CLIENT_RESULT={int(require_client_result)}',
             f'-DLAPY_MAX_REQUESTS={args.max_requests}']
    if rule:
        flags += ['-DLAPY_TITLE_RULE=1',
                  f'-DLAPY_TITLE_PREFIX="{args.title_rule}"',
                  f'-DLAPY_TITLE_ALLOW_LIST="{args.allow_list}"']
    identity = hashlib.sha256(json.dumps({"inputs": inputs,
                                          "sdk": sdk_inputs,
                                          "flags": flags},
                                         sort_keys=True).encode()).hexdigest()
    suffix = "-service" if args.service else ""
    if args.elf_helper:
        suffix = "-helper-" + title
    directory = args.title_rule + "-rule" if rule else title
    if args.service and args.require_client_result:
        suffix += "-verified-client"
    if args.max_requests:
        suffix += f"-max{args.max_requests}"
    output = ROOT / ("build/owned_root_helper-" + directory if args.elf_helper
                     else "build/owned_root_daemon" + suffix)
    output.mkdir(parents=True, exist_ok=True)
    (output / "owned_identity.h").write_text(
        '#define LAPY_OWNED_ID "' + identity + '"\n')
    elf = output / ("lapy.elf" if args.elf_helper
                    else "lapy-root-daemon.elf")
    elf.unlink(missing_ok=True)
    command = [str(sdk / "bin/prospero-clang"), *flags, "-I" + str(logging),
               "-I" + str(output), "-I" + str(ROOT / "source"),
               str(paths[0]), str(paths[1]), str(paths[2]),
               *([str(ROOT / "source/title_rule.c")] if rule else []), "-o", str(elf)]
    with (output / "build.log").open("w") as log:
        log.write(json.dumps(command) + "\n")
        log.flush()
        subprocess.run(command, check=True, stdout=log, stderr=log)
    if elf.read_bytes()[:4] != b"\x7fELF":
        raise RuntimeError("output is not ELF")
    mode = "elf-helper" if args.elf_helper else "service" if args.service else "one-shot"
    manifest = {"schema": "lapy-owned-build/1", "build_id": identity,
                "firmware": "SDK-supported, runtime layout check required",
                "validated_firmware": ["12.02"], "target_title": title,
                "mode": mode,
                "max_requests": (args.max_requests or None) if args.service else 1,
                "service": args.service,
                "require_client_result": require_client_result,
                "console_validated": False,
                "elf_sha256": digest(elf), "inputs_sha256": inputs,
                "sdk_inputs_sha256": sdk_inputs}
    if rule:
        manifest["title_rule"] = {"prefix": args.title_rule,
                                  "allow_list": args.allow_list}
    if args.elf_helper:
        manifest["protocol_sha256"] = digest(
            ROOT / "source" / "lapy_elevation_protocol.h")
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    if args.elf_helper:
        release_manifest = {key: manifest[key] for key in (
            "schema", "build_id", "firmware", "validated_firmware",
            "target_title", "mode", "max_requests", "service",
            "require_client_result", "console_validated", "elf_sha256",
            "protocol_sha256")}
        release_manifest["features"] = ["root_layout_probe_retry"]
        if rule:
            release_manifest["title_rule"] = manifest["title_rule"]
            release_manifest["features"].append("title_rule")
        (output / "lapy-manifest.json").write_text(
            json.dumps(release_manifest, indent=2, sort_keys=True) + "\n")
    print(f"Built {elf.relative_to(ROOT)}\nmode={mode}\nbuild_id={identity}\n"
          f"sha256={manifest['elf_sha256']}")


if __name__ == "__main__":
    main()
