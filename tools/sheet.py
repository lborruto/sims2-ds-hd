"""Contact sheet of shots: tools/sheet.py OUT.png shot1.png shot2.png ... (labels = file names)."""
import sys
from PIL import Image, ImageDraw
out, files = sys.argv[1], sys.argv[2:]
cols = min(8, len(files)); rows = (len(files) + cols - 1) // cols
W, H = 256, 384 + 14
sheet = Image.new("RGB", (cols * (W + 4), rows * (H + 4)), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    x, y = (i % cols) * (W + 4), (i // cols) * (H + 4)
    sheet.paste(Image.open(f), (x, y + 14))
    d.text((x + 2, y + 1), f.split("/")[-1], fill=(255, 255, 0))
sheet.save(out)
