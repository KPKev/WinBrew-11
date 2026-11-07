# ps5debug-NG review for Skyrim

Review date: 2026-10-29. Source consulted: https://github.com/Pharaoh2k/ps5debug-NG (master, not pinned to a commit); this is not an audit of a verified release binary. No debugger was installed or run.

The upstream README lists PS5 firmware 13.60 as verified by its authors. `installer/source/main.c` calls `install_kernel_patch`, `unlock_target_syscall_filter` on SceShellCore, and `sys_proc_call_remote_func` to inject the embedded server. Thus even an apparently read-only query requires system-level state changes to start the tool. Its documented debugger attach uses ptrace and resumes the target; software breakpoints write process memory. It exposes a network command interface with capabilities beyond observation; authentication and containment must be assessed against pinned source and local network access before use.

Timing matters: Skyrim reads the descriptor before opening the main runtime. An attach after a short-lived process appears may occur too late. Upstream describes attach, registers and breakpoints, but no demonstrated launch-at-entry workflow or captured error for FAKE78005. A claim of firmware support does not establish suitability or safe recovery on this individual PS5. Kernel-patch restoration, server shutdown, and behavior after partial injection or reboot have not been verified. No recommendation to install is made.

References: https://raw.githubusercontent.com/Pharaoh2k/ps5debug-NG/master/installer/source/main.c and https://github.com/Pharaoh2k/ps5debug-NG/blob/master/PROTOCOL.md.
