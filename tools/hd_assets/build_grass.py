#!/usr/bin/env python3
"""Makes the HD grass the client lays under the ground (Client/CONTENTS/world/grass.png).

Run from the repo root:  python3 tools/hd_assets/build_grass.py
(only the picture changes: no rebuild of the game is needed, just restart the client)

Reads   tools/hd_assets/source/grass_blades_hi3d.png   close-up blades   (Hi3D, Nano-Banana-Pro)
        tools/hd_assets/source/grass_far_hi3d.png      the meadow from high up (same)
Writes  Client/CONTENTS/world/grass.png                64 picture pixels = one ground tile

The scale is set against the character, who stands about 1.5 tiles (48 game pixels) tall:
  BLADE_TILES   how many tiles the close-up picture is shrunk to cover. Its blades are about a
                ninth of the picture long, so 4 tiles makes a blade about 14 game pixels: a bit
                over a quarter of the character, shin-high wild grass. Smaller = finer grass.
  TILES         the size of the finished picture in tiles (how often the ground repeats).
How it is put together:
  * both sources are made to repeat (their left / top BAND pixels cross-faded with what lies
    beyond the right / bottom edge, and cut down by that band)
  * the blades are shrunk and scattered: hundreds of round pieces taken from random places in
    them, each turned or mirrored at random, so the ground has no rows or repeats of its own
  * the far picture's broad light and dark patches are laid over them, so no two repeats of the
    blades look the same
  * the colour is moved to the old grass's (maptiles1 sheet 0: mean R 48 G 62 B 13), a little
    richer, so the old grass-to-dirt edge tiles still meet it
"""
import os
import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'tools', 'hd_assets', 'source')
OUT = os.path.join(ROOT, 'Client', 'CONTENTS', 'world', 'grass.png')
BLADE_TILES, TILES, BAND = 4, 16, 128
PATCHES = 0.9            # how strongly the far picture's patches show (0 = not at all)
TARGET = np.array([50.0, 66.0, 17.0])
SPLAT, SPLATS, SEED = 112, 900, 7   # the scatter below: piece size in picture pixels, how many, which shuffle

def repeating(name):
    a = np.asarray(Image.open(os.path.join(SRC, name)).convert('RGB')).astype(np.float32)
    H, W, _ = a.shape
    f = np.arange(BAND, dtype=np.float32) / BAND
    f = f * f * (3 - 2 * f)
    a[:, :BAND] = a[:, :BAND] * f[None, :, None] + a[:, W - BAND:] * (1 - f)[None, :, None]
    a = a[:, :W - BAND]
    a[:BAND] = a[:BAND] * f[:, None, None] + a[H - BAND:] * (1 - f)[:, None, None]
    return a[:H - BAND]

def img(a): return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

cell = 64 * BLADE_TILES
blades = np.asarray(img(repeating('grass_blades_hi3d.png')).resize((cell, cell), Image.LANCZOS)
                    .filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=0))).astype(np.float32)
size = 64 * TILES
# scatter: SPLATS round pieces of the shrunk blades, each from a random place in it and turned a
# random quarter turn or mirrored, laid at random over the picture (wrapping round its edges).
# Where pieces overlap the strongest one shows, with a narrow soft join, so nothing lines up in rows.
rng = np.random.default_rng(SEED)
R = SPLAT // 2
yy, xx = np.mgrid[-R:R, -R:R].astype(np.float32)
mask = np.clip(1 - np.sqrt(xx * xx + yy * yy) / R, 0, 1) ** 2
acc = np.zeros((size, size, 3), np.float32)
wsum = np.zeros((size, size), np.float32)
for i in range(SPLATS):
    sy, sx = rng.integers(0, cell, 2)
    piece = np.take(np.take(blades, np.arange(sy, sy + SPLAT) % cell, axis=0), np.arange(sx, sx + SPLAT) % cell, axis=1)
    piece = np.rot90(piece, int(rng.integers(0, 4)))
    if rng.integers(0, 2): piece = piece[:, ::-1]
    w = (mask * float(rng.uniform(0.6, 1.0))) ** 6
    ys = np.arange(SPLAT)[:, None] + int(rng.integers(0, size)); xs = np.arange(SPLAT)[None, :] + int(rng.integers(0, size))
    ys %= size; xs %= size
    acc[ys, xs] += piece * w[:, :, None]
    wsum[ys, xs] += w
assert wsum.min() > 0, 'holes: more SPLATS needed'
out = acc / wsum[:, :, None]

far = img(repeating('grass_far_hi3d.png')).resize((size, size), Image.LANCZOS)
big = np.asarray(Image.fromarray(np.tile(np.asarray(far), (3, 3, 1))).filter(ImageFilter.GaussianBlur(20)))
low = big[size:2 * size, size:2 * size].astype(np.float32)        # blurred with its neighbours: still repeats
gain = low / low.reshape(-1, 3).mean(0)[None, None, :]
out *= 1 + PATCHES * (gain - 1)
out *= (TARGET / out.reshape(-1, 3).mean(0))[None, None, :]
img(out).convert('RGBA').save(OUT)
print(size, np.clip(out, 0, 255).reshape(-1, 3).mean(0).round(1))
