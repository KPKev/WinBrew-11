#!/bin/bash
set -euo pipefail
repo="$PWD"
work="$HOME/counterstrike-wine019-work"
export PATH="$work/toolchain/usr/bin:$PATH"
export M4="$work/toolchain/usr/bin/m4"
export BISON_PKGDATADIR="$work/toolchain/usr/share/bison"
mkdir -p "$work/host-build"
cd "$work/host-build"
"$work/wine-base/configure" --enable-win64 --disable-tests --without-mingw --without-x --without-freetype --without-alsa --without-pulse --without-gstreamer --without-dbus > "$work/host-configure.log" 2>&1
make -j8 __tooldeps__ > "$work/host-tools.log" 2>&1
printf 'HOST TOOLS READY\n'
