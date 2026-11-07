"""Original shell graphics and a Selawik raster atlas (SIL OFL 1.1)."""
from pathlib import Path
import shutil
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
out=root/'assets';out.mkdir(exist_ok=True)
fontroot=root/'vendor/selawik'
regular=next(fontroot.rglob('selawk.ttf'))
semibold=next(fontroot.rglob('selawksb.ttf'))
data=bytearray()
for size,path in [(18,regular),(20,semibold),(28,semibold),(38,regular)]:
    f=ImageFont.truetype(str(path),size)
    for c in range(32,128):
        im=Image.new('L',(48,56));d=ImageDraw.Draw(im)
        d.text((0,-2),chr(c),font=f,fill=255,anchor='la')
        b=bytearray(im.tobytes());b[0]=round(f.getlength(chr(c)));data+=b
(out/'font.bin').write_bytes(data)
# Original generated artwork, retained as an editable source-size PNG.
im=Image.open(root/'art/blue-bloom-source.png').convert('RGBA')
im=im.resize((1920,1080),Image.Resampling.LANCZOS)
im.save(out/'desktop.png');(out/'desktop.rgba').write_bytes(im.tobytes())
sce=root/'sce_sys';sce.mkdir(exist_ok=True)
from icon_design import render_icon
render_icon(root, sce/'icon0.png')
from make_brand import build_brand
build_brand(root)
from make_icon_atlas import build_icons
build_icons(root)
(root/'licenses').mkdir(exist_ok=True)
shutil.copy2(fontroot/'LICENSE.txt',root/'licenses/Selawik-OFL.txt')
print('Generated original desktop, title icon and four-size Selawik atlas')
