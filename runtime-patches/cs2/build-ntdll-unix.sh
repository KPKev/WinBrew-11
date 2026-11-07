#!/bin/bash
set -euo pipefail
repo="$PWD"
work="$HOME/counterstrike-wine019-work"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PATH="$work/toolchain/usr/bin:$llvm/lib/llvm-18/bin:$PATH"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export M4="$work/toolchain/usr/bin/m4" BISON_PKGDATADIR="$work/toolchain/usr/share/bison"
sdk="$repo/build/return-wrapper/foundation/.deps/native/ps5-payload-sdk"
export PS5_PAYLOAD_SDK="$sdk"
mkdir -p "$work/ps5-build" "$work/ps5lib" "$work/objects"
cd "$work/ps5-build"
cp "$work/prospero/wine/ps5/menu/pw_title_menu.h" "$work/wine-source/dlls/ntdll/pw_title_menu.h"
if [ -f config.status ]; then
 ./config.status > "$work/ps5-config-status.log" 2>&1
else
"$work/wine-source/configure" --host=x86_64-unknown-freebsd11 --build=x86_64-pc-linux-gnu \
 --enable-archs=i386,x86_64 --disable-tests --without-x --without-fontconfig --without-freetype \
 --without-alsa --without-pulse --without-dbus --without-gstreamer --without-sdl --without-udev --without-usb \
 --without-v4l2 --without-wayland --without-opengl --without-oss --without-pcap --without-pcsclite \
 --without-sane --without-krb5 --without-gphoto --without-netapi --without-inotify --without-capi \
 --without-cups --without-gssapi --without-ffmpeg --without-gnutls --with-wine-tools="$work/host-build" \
 CC="$sdk/bin/prospero-clang" CFLAGS="-g -O2 -ffile-prefix-map=$work=/build/counterstrike-wine019 -fmacro-prefix-map=$work=/build/counterstrike-wine019" > "$work/ps5-configure.log" 2>&1
fi
printf 'PS5 CONFIGURED\n'
for lib in libm libpthread; do "$sdk/bin/llvm-ar" rcs "$work/ps5lib/$lib.a"; done
sources="$work/prospero/wine/ps5"
extra=()
for unit in pw_wine_heap pw_wine_heap_libc pw_wine_dmem pw_wine_sections pw_wine_dmem_ps5; do
 "$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror -fPIC -c "$sources/$unit.c" -o "$work/objects/$unit.o"
 extra+=("$work/objects/$unit.o")
done
rm -f "$work/ps5-build/dlls/ntdll/ntdll.so"
make -j8 LDFLAGS="${extra[*]} -L$work/ps5lib -lunwind -Wl,--warn-unresolved-symbols" dlls/ntdll/ntdll.so > "$work/ntdll-make.log" 2>&1
printf 'NTDLL UNIX TARGET BUILT\n'
