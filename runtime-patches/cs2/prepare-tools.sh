#!/bin/bash
set -euo pipefail
work="$HOME/counterstrike-wine019-work"
mkdir -p "$work/debs" "$work/toolchain"
cd "$work/debs"
apt-get download bison flex m4
for package in ./*.deb; do dpkg-deb -x "$package" "$work/toolchain"; done
printf "LOCAL DEPS\n"
ls -l "$work/toolchain/usr/bin/"{bison,flex,m4}
ldd "$work/toolchain/usr/bin/m4"
