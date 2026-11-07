#!/usr/bin/env python3
"""Make a snapshot-only CloudService with Playden's shared UID guard.

The upstream source is not changed. Re-check the transformation whenever the
pinned source changes. A guard covers the full request, including status writes.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: prepare-steam-cloud-experiment.py OUTPUT")
source = (root / "vendor/playden-source/src/steam/cloud_service.cpp").read_text()
include = '#include "steam/cloud_service.h"'
boundary = '      request = std::move(pending_.front());\n      pending_.pop_front();\n    }\n    if (request.kind == Kind::kRestore) {'
if source.count(include) != 1 or source.count(boundary) != 1:
    raise SystemExit("Unexpected CloudService source; refusing ambiguous transformation")
source = source.replace(include, include + '\n#include "platform/ps5/title_allow_list.h"', 1)
source = source.replace(boundary, boundary.replace('    if (request.kind',
    '    const auto uid = playden::ps5::HoldEffectiveUid();\n    if (request.kind'), 1)
out = Path(sys.argv[1])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(source)
