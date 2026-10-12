from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
image = Image.new('RGB', (320, 200), (12, 12, 15))
draw = ImageDraw.Draw(image)
def text(y, s, size, colour):
    font = ImageFont.truetype('/System/Library/Fonts/Menlo.ttc', size)
    box = draw.textbbox((0, 0), s, font=font)
    draw.text(((320-box[2])/2, y), s, font=font, fill=colour)
draw.rectangle((8, 8, 311, 191), outline=(110, 30, 25))
text(22, 'MITH DOOM', 30, (220, 60, 40))
text(59, 'MITH.xyz', 12, (210, 160, 125))
text(85, 'CHOOSE A DOOM WAD', 15, (245, 225, 185))
text(112, 'Game > WAD File', 12, (180, 180, 185))
text(147, 'Map keys using Piano mode', 10, (130, 130, 140))
text(166, 'Check Game > Status for errors', 9, (130, 130, 140))
pixels = [(r<<16)|(g<<8)|b for r, g, b in image.getdata()]
(root/'src/Splash.h').write_text('#pragma once\n#include <stdint.h>\nstatic const uint32_t splash[64000] = {\n'+
    '\n'.join(','.join(hex(p) for p in pixels[i:i+20])+',' for i in range(0, len(pixels), 20))+'\n};\n')
image.save(root/'previews/choose-wad.png')
