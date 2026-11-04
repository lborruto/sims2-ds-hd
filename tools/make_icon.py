"""Draws the app icon (packaging/icon.png, 1024 px, and packaging/icon.ico for Windows).

An original picture of the layout itself: a wide world view with the touch panel beside it.
Needs Pillow: python3 tools/make_icon.py
"""
from pathlib import Path
from PIL import Image, ImageDraw

N = 1024
out = Path(__file__).resolve().parent.parent / "packaging"
img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# Background tile (macOS-style margins: the artwork sits inside 824 px).
d.rounded_rectangle((100, 100, 924, 924), radius=185, fill=(26, 32, 48, 255))

# The world view: sky to grass, a sun, rolling hills.
wx0, wy0, wx1, wy1 = 170, 300, 690, 690
world = Image.new("RGBA", (wx1 - wx0, wy1 - wy0))
wd = ImageDraw.Draw(world)
h = wy1 - wy0
for y in range(h):
    t = y / h
    if t < 0.62:
        c = (int(92 + 70 * t), int(160 + 60 * t), int(230 - 20 * t))
    else:
        u = (t - 0.62) / 0.38
        c = (int(84 - 30 * u), int(170 - 40 * u), int(70 - 20 * u))
    wd.line((0, y, wx1 - wx0, y), fill=c + (255,))
wd.ellipse((360, 40, 450, 130), fill=(255, 224, 120, 255))
wd.ellipse((-120, 200, 300, 520), fill=(70, 150, 60, 255))
wd.ellipse((180, 230, 640, 560), fill=(60, 135, 55, 255))
mask = Image.new("L", world.size, 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, world.width - 1, world.height - 1), radius=36, fill=255)
img.paste(world, (wx0, wy0), mask)

# The touch panel beside it: a menu with four buttons.
px0, py0, px1, py1 = 720, 400, 854, 590
d.rounded_rectangle((px0, py0, px1, py1), radius=24, fill=(122, 130, 214, 255))
for i in range(4):
    y = py0 + 22 + i * 40
    d.rounded_rectangle((px0 + 18, y, px1 - 18, y + 24), radius=12, fill=(214, 220, 255, 255))

img.save(out / "icon.png")
img.save(out / "icon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("wrote", out / "icon.png", out / "icon.ico")
