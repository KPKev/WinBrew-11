import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_branding_identity_and_compatibility():
    metadata = json.loads((ROOT / 'sce_sys/param.json').read_text())
    assert metadata['localizedParameters']['en-US']['titleName'] == 'WinBrew 11'
    assert metadata['titleId'] == 'PPSA77381'
    assert metadata['contentId'] == 'UP9000-PPSA77381_00-WINE11DESKTOP001'

    svg = (ROOT / 'sce_sys/icon0.svg').read_text()
    assert svg.count('<linearGradient') == 4 and 'WinBrew 11</text>' not in svg
    brand = (ROOT / 'src/brand_asset.hpp').read_text()
    assert 'inline constexpr uint32_t tile[16384]' in brand
    assert 'inline constexpr uint32_t mark[4096]' in brand
    icons = (ROOT / 'src/icons.cpp').read_text()
    assert 'if (kind == 0)' in icons and 'brand_icon(x, y, size);' in icons

    updater = (ROOT / 'tools/update_desktop.py').read_text()
    assert "('Wine 11', 'WinBrew 11')" in updater
    assert "for name in ('lapy.elf', 'lapy-manifest.json', 'sce_module/libc.prx'):" in updater

    visible_sources = ['desktop_ui.cpp', 'tray_ui.cpp', 'host.cpp', 'ps5.cpp',
                       'explorer.cpp', 'paint.cpp']
    for name in visible_sources:
        assert 'Wine 11' not in (ROOT / 'src' / name).read_text(), name
    native_generator = (ROOT / 'tools/prepare-steam-native-experiment.py').read_text()
    assert 'close WinBrew 11 from the PS5 menu' in native_generator
    assert 'close Wine 11 from the PS5 menu' not in native_generator
    native_builder = (ROOT / 'tools/build-steam-native-ui-probe.sh').read_text()
    assert "replace('Hello World:', 'WinBrew 11:')" in native_builder
    assert "Playden's Wine-based runtime" in (ROOT / 'src/desktop_ui.cpp').read_text()
    assert (ROOT / 'sce_sys/icon0.png').stat().st_size > 1000


if __name__ == '__main__':
    test_branding_identity_and_compatibility()
    print('PASS: WinBrew 11 identity, icon and installed-data compatibility')
