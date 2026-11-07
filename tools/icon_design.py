"""Deterministic PS5 Home artwork matching the taskbar Start mark."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

COLORS = (((96,207,255),(63,170,226)), ((51,174,243),(24,139,219)),
          ((49,157,230),(0,120,215)), ((10,132,221),(0,119,211)))

def render_icon(root: Path, destination: Path):
    scale = 4
    image = Image.new('RGB', (512 * scale, 512 * scale), 'white')
    pane, gap, origin = 112, 8, 140
    for quadrant, (start, end) in enumerate(COLORS):
        row, col = divmod(quadrant, 2)
        yy, xx = np.mgrid[0:pane*scale, 0:pane*scale].astype(np.float32)
        t = (xx + yy) / (2 * (pane*scale - 1))
        pixels = np.empty((pane*scale, pane*scale, 3), dtype=np.uint8)
        for c in range(3): pixels[:,:,c] = start[c] + (end[c]-start[c])*t
        tile = Image.fromarray(pixels).convert('RGBA')
        mask = Image.new('L', tile.size, 255)
        draw = ImageDraw.Draw(mask)
        # Only the four outer corners are rounded, just like the taskbar mark.
        radius = 5 * scale
        cx = 0 if col == 0 else pane*scale-radius
        cy = 0 if row == 0 else pane*scale-radius
        draw.rectangle((cx,cy,cx+radius-1,cy+radius-1),fill=0)
        draw.pieslice((cx if col == 0 else cx-radius, cy if row == 0 else cy-radius,
                       cx+2*radius-1 if col == 0 else cx+radius-1,
                       cy+2*radius-1 if row == 0 else cy+radius-1),
                      180 if (col,row)==(0,0) else 270 if (col,row)==(1,0) else 90 if (col,row)==(0,1) else 0,
                      270 if (col,row)==(0,0) else 360 if (col,row)==(1,0) else 180 if (col,row)==(0,1) else 90,fill=255)
        image.paste(tile, ((origin+col*(pane+gap))*scale,(origin+row*(pane+gap))*scale), mask)
    image.resize((512,512), Image.Resampling.LANCZOS).save(destination)
    paths=[]
    for i,(a,b) in enumerate(COLORS):
        x=origin+(i%2)*(pane+gap); y=origin+(i//2)*(pane+gap)
        paths.append(f'<rect x="{x}" y="{y}" width="112" height="112" fill="url(#g{i})"/>')
    gradients=''.join(f'<linearGradient id="g{i}" x2="1" y2="1"><stop stop-color="rgb{a}"/><stop offset="1" stop-color="rgb{b}"/></linearGradient>' for i,(a,b) in enumerate(COLORS))
    (destination.parent/'icon0.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512"><defs>{gradients}</defs><rect width="512" height="512" fill="white"/>{"".join(paths)}</svg>',encoding='utf-8')
