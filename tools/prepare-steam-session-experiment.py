#!/usr/bin/env python3
"""Make a snapshot-only session repository with a consistent UID lock order.

The original Playden source is untouched. Every public session operation
acquires the process-wide UID guard before the shared session-vault mutex.
"""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2: raise SystemExit("usage: prepare-steam-session-experiment.py OUTPUT")
source = (root / "vendor/playden-source/src/steam/local_session_repository.cpp").read_text()
include = '#include "steam/local_session_repository.h"'
if source.count(include) != 1: raise SystemExit("Unexpected session repository source")
source = source.replace(include, include + '\n#include "platform/ps5/title_allow_list.h"', 1)
for signature in (
    'Result<std::optional<StoredSession>> LocalSessionRepository::Load() const {',
    'Result<void> LocalSessionRepository::Save(const StoredSession& session) const {',
    'Result<void> LocalSessionRepository::Clear() const {',
    'Result<void> LocalSessionRepository::SetPersist(bool persist) const {',
):
    if source.count(signature) != 1: raise SystemExit("Unexpected session operation: " + signature)
    source = source.replace(signature, signature + '\n  const auto uid = ps5::HoldEffectiveUid();', 1)
out = Path(sys.argv[1]); out.parent.mkdir(parents=True, exist_ok=True); out.write_text(source)
