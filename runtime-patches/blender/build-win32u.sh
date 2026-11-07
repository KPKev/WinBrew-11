#!/bin/bash
set -euo pipefail
repo="$PWD"
work="${BLENDER_WINE_WORK:-$HOME/blender-wine019-work}"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PATH="$work/toolchain/usr/bin:$llvm/lib/llvm-18/bin:$PATH"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
export M4="$work/toolchain/usr/bin/m4" BISON_PKGDATADIR="$work/toolchain/usr/share/bison"
sdk="${PS5_PAYLOAD_SDK:-$repo/build/return-wrapper/foundation/.deps/native/ps5-payload-sdk}"
export PS5_PAYLOAD_SDK="$sdk"
fail(){ echo "$*"; exit 1; }
FREETYPE_VERSION=2.13.3
FREETYPE_SHA256=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
FREETYPE_URL=https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VERSION.tar.xz
# Only what Wine's fonts need: TrueType/CFF/Type 1 outlines, Windows .fon
# bitmaps, hinting and the two rasterisers; gzip (FreeType's own zlib copy)
# for the compressed WOFF tables sfnt reads.
FREETYPE_UNITS="base/ftsystem base/ftinit base/ftdebug base/ftbase base/ftbbox base/ftbitmap
 base/ftglyph base/ftmm base/ftsynth base/fttype1 base/ftfstype base/ftgasp base/ftwinfnt
 autofit/autofit truetype/truetype type1/type1 cff/cff winfonts/winfnt psaux/psaux
 psnames/psnames pshinter/pshinter sfnt/sfnt smooth/smooth raster/raster gzip/ftgzip"
# The functions dlls/win32u/freetype.c and dlls/dwrite/freetype.c load with
# dlsym.
FREETYPE_EXPORTS="FT_Activate_Size FT_Done_Face FT_Done_FreeType FT_Done_Glyph FT_Done_Size
 FT_Get_Glyph FT_Get_Kerning FT_Glyph_Copy FT_Glyph_Get_CBox FT_Glyph_Transform FT_New_Size
 FT_Outline_Copy FT_Outline_Decompose FT_Outline_Done FT_Outline_EmboldenXY FT_Outline_New
 FT_Get_Char_Index FT_Get_First_Char FT_Get_Next_Char
 FT_Get_Sfnt_Name FT_Get_Sfnt_Name_Count FT_Get_Sfnt_Table FT_Get_TrueType_Engine_Type
 FT_Get_WinFNT_Header FT_Init_FreeType FT_Library_SetLcdFilter FT_Library_Version
 FT_Load_Glyph FT_Load_Sfnt_Table FT_Matrix_Multiply FT_MulDiv FT_MulFix FT_New_Face
 FT_New_Memory_Face FT_Outline_Embolden FT_Outline_Get_Bitmap FT_Outline_Get_CBox
 FT_Outline_Transform FT_Outline_Translate FT_Property_Set FT_Render_Glyph FT_Set_Charmap
 FT_Set_Pixel_Sizes FT_Vector_Length FT_Vector_Transform FT_Vector_Unit"
ft=$work/freetype
ft_stamp=$(printf '%s\n' "$FREETYPE_SHA256" "$FREETYPE_UNITS" | sha256sum | cut -c1-64)
if [ "$(cat "$ft/.prospero-stamp" 2>/dev/null)" != "$ft_stamp" ]; then
    rm -rf "$ft"
    mkdir -p "$ft/src" "$ft/obj" "$ft/config"
    tarball=${PROSPERO_FREETYPE_TARBALL:-$ft/freetype-$FREETYPE_VERSION.tar.xz}
    [ -f "$tarball" ] || curl -sSfL -o "$tarball" "$FREETYPE_URL" ||
        fail "cannot download $FREETYPE_URL"
    [ "$(sha256sum "$tarball" | cut -c1-64)" = "$FREETYPE_SHA256" ] ||
        fail "$tarball does not match FreeType $FREETYPE_VERSION's pinned SHA-256"
    tar -xJf "$tarball" -C "$ft/src" --strip-components=1
    cat > "$ft/config/ftmodule_ps5.h" <<'EOF'
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, t1_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, winfnt_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )
EOF
    for unit in $FREETYPE_UNITS; do
        "$sdk/bin/prospero-clang" -std=c99 -O2 -fPIC -DFT2_BUILD_LIBRARY \
            '-DFT_CONFIG_MODULES_H=<ftmodule_ps5.h>' -I"$ft/config" -I"$ft/src/include" \
            -c "$ft/src/src/$unit.c" -o "$ft/obj/$(basename "$unit").o" ||
            fail "cannot compile FreeType $unit.c"
    done
    "$sdk/bin/llvm-ar" rcs "$ft/libfreetype.a" "$ft"/obj/*.o
    echo "$ft_stamp" > "$ft/.prospero-stamp"
    echo "built FreeType $FREETYPE_VERSION"
fi

build="$work/blender-ps5-build"
mkdir -p "$build"
cd "$build"
if [ ! -f config.status ]; then
"$work/wine-source/configure" --host=x86_64-unknown-freebsd11 --build=x86_64-pc-linux-gnu \
 --enable-archs=i386,x86_64 --disable-tests --without-x --without-fontconfig --with-freetype \
 --without-alsa --without-pulse --without-dbus --without-gstreamer --without-sdl --without-udev --without-usb \
 --without-v4l2 --without-wayland --without-opengl --without-oss --without-pcap --without-pcsclite \
 --without-sane --without-krb5 --without-gphoto --without-netapi --without-inotify --without-capi \
 --without-cups --without-gssapi --without-ffmpeg --without-gnutls --with-wine-tools="$work/host-build" \
 CC="$sdk/bin/prospero-clang" CFLAGS="-g -O2 -ffile-prefix-map=$work=/build/blender-wine019" \
 FREETYPE_CFLAGS="-I$ft/src/include" FREETYPE_LIBS="$ft/libfreetype.a" \
 ac_cv_lib_soname_freetype=libfreetype.so ac_cv_lib_soname_vulkan=libvulkan.so > "$work/blender-configure.log" 2>&1
fi
make -j8 LDFLAGS="-L$work/ps5lib -lunwind -Wl,--warn-unresolved-symbols" dlls/win32u/win32u.so > "$work/blender-win32u-make.log" 2>&1
printf 'BLENDER WIN32U BUILT\n'
