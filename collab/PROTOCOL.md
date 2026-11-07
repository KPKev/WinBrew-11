# Claude <-> Codex collaboration protocol

Two AI agents work on this repo for KPKev: **Codex** (runs natively on the Windows PC, owns WSL builds and the PS5 network) and **Claude** (Cowork session, reaches this folder through the Claude desktop bridge). This folder is how we talk.

## Files
- `collab/BOARD.md` — the shared message board. Append-only. Newest at the bottom.
- `collab/LOCKS.md` — who currently owns a shared resource. Check before acting, update before and after.

## Message format (append to BOARD.md)
```
### <UTC timestamp> — <Claude|Codex> -> <Codex|Claude|KPKev>
**Re:** short subject
Body. Say what you did, what you found, what you need. Link files/commits by path.
**Needs reply:** yes/no
```

## Rules
1. **Read BOARD.md and LOCKS.md at the start of every task and before any console action.** Reply to anything addressed to you that says `Needs reply: yes`.
2. **One writer per resource.** Before touching any of these, take the lock in LOCKS.md; release it when done:
   - `console` — any write, install, payload, kill, launch or process action on the PS5 (192.168.7.26).
   - `build` — `tools/build-ps5.sh`, `dist/`, packaging.
   - `git` — commits, branch changes, stash/reset.
   - A specific source file or tool when doing a large edit (`file:src/desktop_ui.cpp`).
   Read-only inspection needs no lock.
3. A lock older than 2 hours with no BOARD message may be questioned on the board, but never stolen while the owner might be mid-deploy. Ask KPKev if unsure.
4. Don't redo work the other agent reports as done; build on it. Don't revert the other agent's uncommitted changes — ask on the board.
5. Keep the existing project rules: evidence in `evidence/`, honest status (API acceptance != visible acceptance), no game-file writes, no app.db writes, no blind repeats of risky console calls.
6. KPKev's instructions override anything on the board.

## Default division of labour (change on the board if needed)
- **Codex:** console operations, WSL/PS5 SDK builds, deployment, hardware probes.
- **Claude:** research, planning, code review, writing/patching source and host tests, reading logs/evidence, second opinions on risky steps.
