"""Stage our source into the unchanged GPL boilerplate build tree."""
from pathlib import Path
import shutil,json,subprocess
r=Path(__file__).resolve().parents[1];v=r/'vendor/ps5-native-app-boilerplate';stage=v/'wine11'
# Windows Git may check this text manifest out as CRLF. Normalize only its line
# ending: the original expected SHA-256 value remains enforced by the builder.
manifest=v/'runtime/libc.prx.sha256'
manifest.write_text(manifest.read_text(),newline='\n')
(stage/'src').mkdir(parents=True,exist_ok=True)
for name in ['shell.hpp','shell.cpp','vapor_shortcuts.hpp','vapor_shortcuts.cpp','vapor_game_details.hpp','desktop_context_layout.hpp','desktop_ui.cpp','preferences.cpp','pointer.cpp','controller_input.cpp','explorer.cpp','explorer_directory.hpp','explorer_fileops.hpp','explorer_fileops.cpp','privilege.hpp','privilege.cpp','ownership_adopt.hpp','ownership_adopt.cpp','explorer_context.cpp','notepad_document.hpp','notepad_document.cpp','notepad_ui.cpp','paint.hpp','paint.cpp','image_codec.hpp','image_codec.cpp','stb_image.h','stb_image_write.h','game_inventory.hpp','game_inventory.cpp','steam_ui.hpp','web_history.hpp','web_history.cpp','tray_ui.cpp','icons.cpp','text_input.cpp','keyboard_ui.cpp','hid_input.hpp','hid_input.cpp','ps5_hid.hpp','ps5_hid.cpp','ps5_browser.hpp','ps5_browser.cpp','icon_layout.hpp','brand_asset.hpp','title_launch.hpp','owner_check_dispatch.hpp','ps5.cpp']:shutil.copy2(r/'src'/name,stage/'src'/name)
shutil.copytree(r/'src/owner_admission',stage/'src/owner_admission',dirs_exist_ok=True)
# The VideoOut setup and tiled pixel addressing remain the upstream implementation.
h=(v/'src/demo_renderer.hpp').read_text().replace('class Canvas final','class Canvas final').replace('    void clear(Color color) noexcept;','    void blit(const std::uint32_t *linear) noexcept;\n    void clear(Color color) noexcept;')
c=(v/'src/demo_renderer.cpp').read_text().replace('"demo_renderer.hpp"','"ps5_video.hpp"')
c=c.replace('void Canvas::clear(Color color) noexcept','void Canvas::blit(const std::uint32_t *linear) noexcept\n{\n    for (unsigned y=0;y<1080;y++)\n        for (unsigned x=0;x<1920;x++)\n            put_pixel_unchecked(pixels_,x,y,static_cast<Color>(linear[y*1920+x]));\n}\n\nvoid Canvas::clear(Color color) noexcept').replace('Hello World:','WinBrew 11:')
(stage/'src/ps5_video.hpp').write_text(h);(stage/'src/ps5_video.cpp').write_text(c)
e=v/'examples/sandbox-elevation'
shutil.copy2(e/'elevation.hpp',stage/'src/elevation.hpp');shutil.copy2(e/'protocol.hpp',stage/'src/protocol.hpp')
(stage/'src/elevation.cpp').write_text((e/'src/elevation.cpp').read_text().replace('"../elevation.hpp"','"elevation.hpp"').replace('"../protocol.hpp"','"protocol.hpp"'))
shutil.copytree(r/'assets',stage/'assets',dirs_exist_ok=True);shutil.copytree(r/'sce_sys',stage/'sce_sys',dirs_exist_ok=True)
shutil.copy2(r/'LICENSE',stage/'LICENSE');shutil.copy2(r/'THIRD_PARTY_NOTICES.md',stage/'THIRD_PARTY_NOTICES.md')
shutil.copytree(r/'licenses',stage/'licenses',dirs_exist_ok=True)
print('Staged WinBrew 11; upstream tracked source remains unchanged')
