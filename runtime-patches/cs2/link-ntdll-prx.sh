#!/bin/bash
set -euo pipefail
repo="$PWD"
work="$HOME/counterstrike-wine019-work"
root="$work/prospero"
build="$work/ps5-build"
prx="$work/prx"
sdk="$repo/build/return-wrapper/foundation/.deps/native/ps5-payload-sdk"
foundation="$repo/build/counterstrike-ntdll-foundation"
tool="$work/native-tool/ps5-native-tool"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PATH="$work/toolchain/usr/bin:$llvm/lib/llvm-18/bin:$PATH"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PS5_PAYLOAD_SDK="$sdk"
mkdir -p "$prx/obj" "$prx/sce_module"
shims=()
for unit in pw_wine_prx pw_wine_dl pw_wine_dl_libc pw_wine_compat pw_wine_compat_libc pw_wine_sink pw_wine_cwd pw_wine_cwd_libc; do
 "$sdk/bin/prospero-clang" -std=gnu11 -O2 -Wall -Wextra -Werror -fPIC -c "$root/wine/ps5/$unit.c" -o "$prx/obj/$unit.o"
 shims+=("$prx/obj/$unit.o")
done
    python3 "$root/tools/gen_prx_descriptor.py" "$prx/obj/ntdll_desc.c" \
        __wine_main pw_wine_dl_adopt dlopen dlsym dlerror \
        pw_wine_heap_stats pw_wine_heap_malloc pw_wine_heap_free \
        pw_wine_set_present_sink pw_wine_post_input pw_wine_sink_stats \
        pw_wine_present pw_wine_next_input pw_wine_input_fd \
        pw_wine_set_pad pw_wine_rumble pw_wine_pad pw_wine_set_rumble pw_wine_set_menu pw_wine_menu pw_wine_pad_changes \
        pw_wine_set_pad_at pw_wine_rumble_at pw_wine_rumble_peak_at pw_wine_pad_at pw_wine_pad_peek pw_wine_set_rumble_at pw_wine_pads_wanted pw_wine_pads_ask pw_wine_pad_notify \
        pw_wine_set_pad_extra pw_wine_pad_extra pw_wine_set_pad_extra_at pw_wine_pad_extra_at \
        pw_wine_set_audio_sink pw_wine_audio_available pw_wine_audio_output \
        __wine_virtual_stats __wine_virtual_fault_top __wine_ps5_set_output_sink __wine_ps5_memory_stats pw_cwd_set \
        __wine_ps5_set_segv_hook __wine_ps5_set_segv_unresolved_hook pw_wine_set_display_release pw_wine_release_display \
        --optional-from "$build/dlls/ntdll/ntdll.so" --nm "$sdk/bin/prospero-nm" \
        --optional-export __wine_prospero_native_wow64_caps

"$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror -fPIC -I"$root/wine/ps5" -c "$prx/obj/ntdll_desc.c" -o "$prx/obj/ntdll_desc.o"
python3 - "$work/ntdll-make.log" "$work/ntdll-objects.txt" <<'PYCODE'
import shlex,sys,pathlib
s=pathlib.Path(sys.argv[1]).read_text().replace("\\\n", " ")
lines=[l for l in s.splitlines() if "-o dlls/ntdll/ntdll.so " in l]
assert lines,'No ntdll ELF link found'
objects=[x for x in shlex.split(lines[-1]) if x.endswith('.o') and not x.startswith('/')]
assert objects
pathlib.Path(sys.argv[2]).write_text('\n'.join(objects)+'\n')
PYCODE
mapfile -t objects < "$work/ntdll-objects.txt"
cwd_wraps=()
for name in $(grep -o '__wrap_[a-z_]*' "$root/wine/ps5/pw_wine_cwd_libc.c" | sed 's/^__wrap_//' | LC_ALL=C sort -u); do cwd_wraps+=("--wrap=$name"); done
extra=()
for unit in pw_wine_heap pw_wine_heap_libc pw_wine_dmem pw_wine_sections pw_wine_dmem_ps5; do extra+=("$work/objects/$unit.o"); done
cd "$build"
"$sdk/bin/prospero-lld" --shared -Bsymbolic -T "$foundation/tooling/native/ps5-pie.ld" -T "$root/wine/ps5/prx_eh_frame.ld" --eh-frame-hdr -soname ntdll.prx -z defs --warn-unresolved-symbols -L"$work/ps5lib" -o "$prx/ntdll.shared.elf" "${objects[@]}" "${extra[@]}" "${shims[@]}" "$prx/obj/ntdll_desc.o" "${cwd_wraps[@]}" "$sdk/target/lib/libunwind.a" --as-needed "$sdk/target/lib/"*.so > "$work/ntdll-prx-link.log" 2>&1
"$tool" link --module --in "$prx/ntdll.shared.elf" --out "$prx/ntdll.elf" --stub-dir "$sdk/target/lib" --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name ntdll.prx >> "$work/ntdll-prx-link.log" 2>&1
"$tool" self --sign --in "$prx/ntdll.elf" --out "$prx/sce_module/ntdll.prx" >> "$work/ntdll-prx-link.log" 2>&1
cp "$prx/sce_module/ntdll.prx" "$repo/build/counterstrike-ntdll-build/ntdll.prx"
printf 'NTDLL PRX READY\n'
