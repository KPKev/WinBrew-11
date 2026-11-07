#!/bin/bash
set -euo pipefail
repo="$PWD"
work="${BLENDER_WINE_WORK:-$HOME/blender-wine019-work}"
root="$work/prospero"
build="$work/blender-ps5-build"
prx="$work/blender-prx"
sdk="${PS5_PAYLOAD_SDK:-$repo/build/return-wrapper/foundation/.deps/native/ps5-payload-sdk}"
foundation="${BLENDER_PRX_FOUNDATION:-$repo/build/counterstrike-ntdll-foundation}"
tool="${BLENDER_NATIVE_TOOL:-$work/native-tool/ps5-native-tool}"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PATH="$work/toolchain/usr/bin:$llvm/lib/llvm-18/bin:$PATH"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
mkdir -p "$prx/obj" "$prx/sce_module"
python3 "$root/tools/gen_prx_descriptor.py" "$prx/obj/win32u_desc.c" __wine_unix_lib_init
"$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror -fPIC -I"$root/wine/ps5" -c "$prx/obj/win32u_desc.c" -o "$prx/obj/win32u_desc.o"
python3 - "$work/blender-win32u-make.log" "$work/blender-win32u-objects.txt" <<'PYCODE'
import shlex,sys,pathlib
s=pathlib.Path(sys.argv[1]).read_text().replace("\\\n", " ")
lines=[l for l in s.splitlines() if "-o dlls/win32u/win32u.so " in l]
assert lines, "No win32u ELF link"
objects=[x for x in shlex.split(lines[-1]) if x.endswith('.o') and not x.startswith('/')]
assert len(objects)>10
pathlib.Path(sys.argv[2]).write_text('\n'.join(objects)+'\n')
print("win32u objects",len(objects))
PYCODE
mapfile -t objects < "$work/blender-win32u-objects.txt"
cd "$build"
"$sdk/bin/prospero-lld" --shared -Bsymbolic -T "$foundation/tooling/native/ps5-pie.ld" -T "$root/wine/ps5/prx_eh_frame.ld" --eh-frame-hdr -soname win32u.prx -z defs --warn-unresolved-symbols -L"$work/ps5lib" -o "$prx/win32u.shared.elf" "${objects[@]}" "$prx/obj/win32u_desc.o" "$work/prx/ntdll.shared.elf" "$sdk/target/lib/libunwind.a" --as-needed "$sdk/target/lib/"*.so > "$work/blender-win32u-prx-link.log" 2>&1
"$tool" link --module --in "$prx/win32u.shared.elf" --out "$prx/win32u.elf" --stub-dir "$sdk/target/lib" --stub "$work/prx/ntdll.shared.elf" --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name win32u.prx >> "$work/blender-win32u-prx-link.log" 2>&1
"$tool" self --sign --in "$prx/win32u.elf" --out "$prx/sce_module/win32u.prx" >> "$work/blender-win32u-prx-link.log" 2>&1
cp "$prx/sce_module/win32u.prx" "$repo/build/blender-win32u-independent-cursor.prx"
printf 'BLENDER WIN32U PRX READY\n'
