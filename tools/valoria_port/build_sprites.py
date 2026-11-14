#!/usr/bin/env python3
"""Builds valoria_items.pak (and the 2x HD copy) from VALORIA's 128x128 item icons.

    python3 tools/valoria_port/build_sprites.py <VALORIA icons folder> <client SPRITES folder>

Sheet 0: bag icons (fit in 44x44), drawn centred.   Registered as item-pack sprite 23.
Sheet 1: ground icons (fit in 24x24), drawn centred. Registered as item-ground sprite 23.
Frame order comes from sprite_frames.json (written by build_items.py).

Each sheet is a PNG behind a 14-byte 'BM' header, which the client's loader accepts
(CSurface::CreateFromBitmapFile). The top-left pixel is the see-through colour.
"""
import json, os, struct, sys, io
from PIL import Image

KEY = (255, 0, 255)
SHEETS = [44, 24]           # largest side of the icon, normal size; HD is twice this


def to565(c):
    r, g, b = c[:3]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


KEY565 = to565(KEY)


def icon(path, box):
    im = Image.open(path).convert('RGBA')
    bb = im.getbbox()
    if bb: im = im.crop(bb)
    w, h = im.size
    s = box / float(max(w, h))
    im = im.resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
    # flatten: mostly see-through pixels become the key colour, the rest opaque
    out = Image.new('RGB', im.size, KEY)
    px, op = im.load(), out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = px[x, y]
            if a < 110: continue
            if a < 255:                                     # darken soft edges toward black, not the key
                r, g, b = (r * a // 255, g * a // 255, b * a // 255)
            if to565((r, g, b)) == KEY565: g = min(255, g + 8)
            op[x, y] = (r, g, b)
    return out


def sheet(icons, box):
    gap = 2
    imgs = [icon(p, box) for p in icons]
    W = gap + sum(i.size[0] + gap for i in imgs)
    H = gap * 2 + max(i.size[1] for i in imgs)
    canvas = Image.new('RGB', (W, H), KEY)
    brushes, x = [], gap
    for im in imgs:
        w, h = im.size
        canvas.paste(im, (x, gap))
        brushes.append((x, gap, w, h, -(w // 2), -(h // 2)))
        x += w + gap
    buf = io.BytesIO()
    canvas.save(buf, 'PNG', optimize=True)
    png = buf.getvalue()
    return brushes, b'BM' + struct.pack('<I', 14 + len(png)) + b'\0' * 8 + png


def write_pak(path, sheets):
    blobs = []
    for brushes, bmp in sheets:
        hdr = b'<Sprite File Header>'.ljust(100, b'\0')
        body = hdr + struct.pack('<I', len(brushes)) + b''.join(struct.pack('<6h', *b) for b in brushes) + b'\0' * 4 + bmp
        blobs.append(body)
    head = b'<Pak file header>'.ljust(20, b'\0') + struct.pack('<I', len(blobs))
    table, off = b'', len(head) + 8 * len(blobs)
    for b in blobs:
        table += struct.pack('<II', off, len(b))
        off += len(b)
    with open(path, 'wb') as f:
        f.write(head + table + b''.join(blobs))


def main():
    icons_dir, sprites_dir = sys.argv[1], sys.argv[2]
    frames = json.load(open(os.path.join(os.path.dirname(__file__), 'sprite_frames.json')))
    paths = []
    for f in frames:
        p = os.path.join(icons_dir, f['icon'] + '.png')
        if not os.path.exists(p):                          # VALORIA's one missing icon: use a near match
            alt = [n for n in os.listdir(icons_dir) if n.startswith(f['icon'].rsplit('_', 1)[0]) and n.endswith('.png')]
            p = os.path.join(icons_dir, sorted(alt)[0]) if alt else os.path.join(icons_dir, 'mx_dagger.png')
            print('no icon', f['icon'], '-> using', os.path.basename(p))
        paths.append(p)
    for scale, out in ((1, os.path.join(sprites_dir, 'valoria_items.pak')), (2, os.path.join(sprites_dir, 'HD', 'valoria_items.pak'))):
        write_pak(out, [sheet(paths, box * scale) for box in SHEETS])
        print('wrote', out, os.path.getsize(out), 'bytes,', len(paths), 'frames')


if __name__ == '__main__':
    main()
