# Kit 0.19 sources

Every binary in this kit is built from, or taken unmodified from, the sources below. My changes are on the
`main` branch of each `leolobato` fork. The build scripts are in Playden's `tools/kit-h/`.

| Component | Revision | Source | License text |
| --- | --- | --- | --- |
| Wine with the prospero-win patch series, Wine's PS5 runtime (`runtime/wine-wow64/7537eb0-kit-0.19`) | prospero-win `7537eb0113b438e8dc0730e594fa0743203bc7c9` | https://github.com/leolobato/prospero-win/tree/7537eb0113b438e8dc0730e594fa0743203bc7c9 (patches in `wine/patches/`); Wine: https://gitlab.winehq.org/wine/wine | `wine-COPYING.LIB`, `wine-LICENSE`, `prospero-win-LICENSE`, `prospero-win-LICENSING.md` |
| Per-game app shell (`titles/any-title-7537eb0`) | prospero-win `7537eb0113b438e8dc0730e594fa0743203bc7c9` | https://github.com/leolobato/prospero-win/tree/7537eb0113b438e8dc0730e594fa0743203bc7c9 | `prospero-win-LICENSE` |
| Native app tools for the shell (prospero-foundation) | `9c0b994a048521af6fb84c73ded364504fe250e9` | https://github.com/leolobato/prospero-foundation/tree/9c0b994a048521af6fb84c73ded364504fe250e9 (fork of mpereiraesaa/ps5-native-app-boilerplate) | `prospero-foundation-LICENSE` |
| Root helper (PS5-Lapy-JB-Daemon) | `66ab4f10f9a6c6a5a42d05f1d165230c13ae2ab7` | https://github.com/leolobato/PS5-Lapy-JB-Daemon/tree/66ab4f10f9a6c6a5a42d05f1d165230c13ae2ab7 | `PS5-Lapy-JB-Daemon-LICENSE` |
| ps5-payload-sdk (helper build) | v0.42 | https://github.com/ps5-payload-dev/sdk/releases/tag/v0.42 | `GPL-3.0.txt` |
| vkd3d-proton (`runtime/wine-wow64/7537eb0-kit-0.19/lib/vkd3d-proton`), with dxil-spirv `62dbb07` and dxbc-spirv `29c93ae` | 3.0.1, `3b10bd7a7ec6a7347e616cf8bea59333afec2255`, unmodified; built with meson (release, tracing on) and mingw-w64 GCC 13, stripped | https://github.com/HansKristian-Work/vkd3d-proton/tree/3b10bd7a7ec6a7347e616cf8bea59333afec2255 (with its submodules) | `vkd3d-proton-LICENSE`, `vkd3d-proton-COPYING`, `dxil-spirv-LICENSE.MIT`, `dxbc-spirv-LICENSE`, `SPIRV-Cross-LICENSE`, `SPIRV-Tools-LICENSE`, `SPIRV-Headers-LICENSE`, `Vulkan-Headers-LICENSE.md` |
| DXVK | 2.6.2, unmodified | https://github.com/doitsujin/dxvk/releases/tag/v2.6.2 | `dxvk-LICENSE` |
| Mesa WGL and Zink (`titles/any-title-7537eb0/win/mesa-zink`) | PS5_Mesa `4778ff0` (my swapchain fix on `e4607ef`) | https://github.com/leolobato/PS5_Mesa/tree/4778ff0a643eb7eb45d43c412d4d3d776444060c | `mesa-license.rst` |
| RADV for the PS5 (`libvulkan.prx`), with my 120 Hz output and wave32 pixel shader changes, linked with prospero-win's Vulkan layer (`wine/ps5/`) and MihawkGen | PS5_Vulkan `8aee3af`, PS5_Mesa `15f78ec35243c567a917b0d3997f60e584fa48ec` (on `01d7057`) | https://github.com/mpereiraesaa/PS5_Vulkan/tree/8aee3af, https://github.com/leolobato/PS5_Mesa/tree/15f78ec35243c567a917b0d3997f60e584fa48ec | `PS5_Vulkan-LICENSE`, `mesa-license.rst` |
| MihawkGen frame generator (in `libvulkan.prx`) | `c57232a9f16bcb6df5fe07c9bb631518f2b1cc0e`, unmodified (vendored in prospero-win `third_party/mihawkgen`) | https://github.com/mihawk-99/PS5_MihawkGen/tree/c57232a9f16bcb6df5fe07c9bb631518f2b1cc0e (`framegen/`) | `mihawkgen-LICENSE.md` |
| OpenAL Soft (`titles/any-title-7537eb0/win/openal-soft`) | 1.25.2, the project's release binaries, unmodified | https://github.com/kcat/openal-soft/releases/tag/1.25.2 (archive SHA-256 `67a0c4b800bd860c93c04f38caf8cbe4875f9c84700ac430efc451f70e265434`) | `openal-soft-COPYING` |
| Wine Mono | 11.3.0, unmodified | https://dl.winehq.org/wine/wine-mono/11.3.0/ (source: https://github.com/wine-mono/wine-mono/tree/wine-mono-11.3.0) | `wine-mono-COPYING` |
| GnuTLS, with libtasn1 | 3.8.13 | https://www.gnutls.org/ | `gnutls-COPYING.LESSERv2` |
| Nettle, with mini-gmp | 3.10.2 | https://www.lysator.liu.se/~nisse/nettle/ | `nettle-COPYING.LESSERv3`, `nettle-COPYINGv2` |
| GStreamer (in `winegstreamer.prx`; LGPL plugins only) | 1.24.12 | https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-1.24.12.tar.xz | `gstreamer-COPYING` |
| gst-plugins-base, gst-plugins-good, gst-plugins-bad, gst-libav | 1.24.12 | https://gstreamer.freedesktop.org/src/ (`gst-plugins-base-1.24.12.tar.xz`, `gst-plugins-good-1.24.12.tar.xz`, `gst-plugins-bad-1.24.12.tar.xz`, `gst-libav-1.24.12.tar.xz`) | `gst-plugins-base-COPYING`, `gst-plugins-good-COPYING`, `gst-plugins-bad-COPYING`, `gst-libav-COPYING` |
| FFmpeg (LGPL build: no `--enable-gpl`, no `--enable-nonfree`) | 7.1.1 | https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz | `ffmpeg-LICENSE.md`, `ffmpeg-COPYING.LGPLv2.1`, `ffmpeg-COPYING.LGPLv3` |
| GLib, with proxy-libintl | 2.80.5 | https://download.gnome.org/sources/glib/2.80/glib-2.80.5.tar.xz | `glib-COPYING`, `glib-proxy-libintl-COPYING` |
| libiconv | 1.17 | https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz | `libiconv-COPYING.LIB`, `libiconv-COPYING` |
| PCRE2 | 10.44 | https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.bz2 | `pcre2-LICENCE`, `pcre2-AUTHORS` |
| libffi | 3.4.6 | https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz | `libffi-LICENSE` |
| zlib | 1.3.1 | https://zlib.net/fossils/zlib-1.3.1.tar.gz | `zlib-LICENSE` |
| FreeType | 2.13.3 | https://freetype.org/ | `freetype-LICENSE.TXT`, `freetype-FTL.TXT` |
| Mbed TLS (in the app shell) | — | https://github.com/Mbed-TLS/mbedtls | `titles/any-title-7537eb0/licenses/mbedtls-publication/LICENSE` inside the kit |
| CA certificate bundle | Mozilla root store (curl's 2026-10-20 extract) | https://curl.se/docs/caextract.html | `MPL-2.0.txt` |

The media components' exact source archives, their SHA-256 and the static archives linked into `winegstreamer.prx` are
listed in the kit at `runtime/wine-wow64/7537eb0-kit-0.19/share/wine/licenses/media/source-manifest.json`.

If a link stops working, open an issue on https://github.com/leolobato/playden-ps5 and I will provide the source.
