# Genera las imágenes de transición de NDE (1920x1080) con dibujo vectorial simple.
# Uso: python generar_transiciones.py <carpeta de salida>  (necesita Pillow)
import math, random, sys
from PIL import Image, ImageDraw, ImageFilter

S = 2
W, H = 1920 * S, 1080 * S
OUT = sys.argv[1]

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))

def gradient(img, stops):
    d = ImageDraw.Draw(img)
    for y in range(H):
        t = y / (H - 1)
        for i in range(len(stops) - 1):
            (t0, c0), (t1, c1) = stops[i], stops[i + 1]
            if t0 <= t <= t1:
                d.line([(0, y), (W, y)], fill=lerp(c0, c1, (t - t0) / (t1 - t0)))
                break

def glow(img, center, radius, color, strength=0.6):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx, cy = center
    for i in range(30, 0, -1):
        r = radius * i / 30 * 3
        a = int(255 * strength * (1 - i / 30) ** 2)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color + (a,))
    layer = layer.filter(ImageFilter.GaussianBlur(radius * 0.4))
    img.alpha_composite(layer)

def stars(img, count, seed, ymax):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    for _ in range(count):
        x, y = rnd.uniform(0, W), rnd.uniform(0, ymax)
        r = rnd.choice([1, 1, 1, 2, 2, 3]) * S * 0.8
        a = rnd.randint(120, 255)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 250, 230, a))

def hill(img, base, amp, color, seed, freq=3):
    rnd = random.Random(seed)
    ph = [rnd.uniform(0, 6.28) for _ in range(3)]
    pts = [(0, H)]
    for x in range(0, W + 20, 20):
        t = x / W
        y = base + amp * (math.sin(t * freq * 3.1 + ph[0]) * 0.6 + math.sin(t * freq * 7.3 + ph[1]) * 0.3 + math.sin(t * 17 + ph[2]) * 0.1)
        pts.append((x, y))
    pts.append((W, H))
    ImageDraw.Draw(img).polygon(pts, fill=color)

def house(img, x, ground, scale, body, roof, window_light, sleeping=True):
    d = ImageDraw.Draw(img)
    w, h = 900 * scale, 560 * scale
    x0, y0 = x - w / 2, ground - h
    # chimenea
    d.rectangle([x0 + w * 0.68, y0 - h * 0.62, x0 + w * 0.78, y0 - h * 0.2], fill=roof)
    # tejado
    d.polygon([(x0 - w * 0.08, y0), (x, y0 - h * 0.62), (x0 + w * 1.08, y0)], fill=roof)
    d.rectangle([x0, y0, x0 + w, ground], fill=body)
    # puerta gigante
    d.rounded_rectangle([x - w * 0.09, ground - h * 0.52, x + w * 0.09, ground], radius=40 * scale, fill=roof)
    # gatera con luz tenue
    d.rounded_rectangle([x - w * 0.025, ground - h * 0.08, x + w * 0.025, ground], radius=10 * scale, fill=(60, 50, 40))
    # ventanas
    wins = [(x0 + w * 0.12, y0 + h * 0.18), (x0 + w * 0.70, y0 + h * 0.18)]
    for i, (wx, wy) in enumerate(wins):
        ww, wh = w * 0.18, h * 0.3
        lit = window_light if i == 1 else (25, 30, 55)
        d.rectangle([wx, wy, wx + ww, wy + wh], fill=lit)
        d.line([(wx + ww / 2, wy), (wx + ww / 2, wy + wh)], fill=roof, width=int(10 * scale))
        d.line([(wx, wy + wh / 2), (wx + ww, wy + wh / 2)], fill=roof, width=int(10 * scale))
        d.rectangle([wx, wy, wx + ww, wy + wh], outline=roof, width=int(14 * scale))
        if i == 1 and sleeping:
            # silueta del gigante dormido tras la ventana
            d.ellipse([wx + ww * 0.15, wy + wh * 0.55, wx + ww * 0.85, wy + wh * 1.05], fill=(90, 60, 40))
            d.ellipse([wx + ww * 0.55, wy + wh * 0.4, wx + ww * 0.85, wy + wh * 0.7], fill=(90, 60, 40))
    return wins[1], (w * 0.18, h * 0.3)

def zzz(img, x, y, scale, color):
    d = ImageDraw.Draw(img)
    for i, s in enumerate([1.0, 1.4, 1.9]):
        sz = 26 * s * scale
        ox, oy = x + i * 46 * scale, y - i * 60 * scale
        lw = int(7 * scale * s)
        d.line([(ox, oy), (ox + sz, oy)], fill=color, width=lw)
        d.line([(ox + sz, oy), (ox, oy + sz)], fill=color, width=lw)
        d.line([(ox, oy + sz), (ox + sz, oy + sz)], fill=color, width=lw)

def van(img, x, ground, scale, color, dark):
    d = ImageDraw.Draw(img)
    w, h = 420 * scale, 200 * scale
    d.rounded_rectangle([x, ground - h - 40 * scale, x + w, ground - 40 * scale], radius=30 * scale, fill=color)
    d.rounded_rectangle([x + w * 0.72, ground - h * 1.0 - 40 * scale, x + w * 1.08, ground - 40 * scale], radius=26 * scale, fill=color)
    d.rectangle([x + w * 0.80, ground - h * 0.9 - 40 * scale, x + w * 1.0, ground - h * 0.5 - 40 * scale], fill=dark)
    for wx in (x + w * 0.2, x + w * 0.85):
        r = 46 * scale
        d.ellipse([wx - r, ground - 40 * scale - r, wx + r, ground - 40 * scale + r], fill=(15, 15, 20))
        d.ellipse([wx - r * 0.4, ground - 40 * scale - r * 0.4, wx + r * 0.4, ground - 40 * scale + r * 0.4], fill=(70, 70, 80))
    # puertas traseras abiertas
    d.rectangle([x - 18 * scale, ground - h - 30 * scale, x + 6 * scale, ground - 50 * scale], fill=dark)

def critter(img, x, ground, scale, species, color, flip=False, run=0.0, sack=False):
    d = ImageDraw.Draw(img)
    f = -1 if flip else 1
    s = scale
    bw, bh = 120 * s, 80 * s
    by = ground - 34 * s - bh
    bx = x - bw / 2
    # cola
    if species in ("raccoon", "fox"):
        tw = 1.0 if species == "raccoon" else 1.3
        pts = [(x - f * bw * 0.4, by + bh * 0.4), (x - f * bw * 1.1 * tw, by - bh * 0.1), (x - f * bw * 1.0 * tw, by + bh * 0.35), (x - f * bw * 0.45, by + bh * 0.75)]
        d.polygon(pts, fill=color)
        if species == "raccoon":
            for k in (0.6, 0.85):
                px = x - f * bw * k * tw
                d.line([(px, by + bh * 0.05), (px + f * 10 * s, by + bh * 0.55)], fill=(0, 0, 0, 255), width=int(8 * s))
    elif species == "mouse":
        d.line([(x - f * bw * 0.45, by + bh * 0.7), (x - f * bw * 1.1, by + bh * 0.2), (x - f * bw * 1.3, by + bh * 0.35)], fill=color, width=int(7 * s))
    elif species == "rabbit":
        d.ellipse([x - f * bw * 0.6 - 18 * s, by + bh * 0.2, x - f * bw * 0.6 + 18 * s, by + bh * 0.6], fill=color)
    if sack:
        sx, sy = x - f * bw * 0.15, by - bh * 0.55
        d.ellipse([sx - 55 * s, sy - 50 * s, sx + 55 * s, sy + 55 * s], fill=(78, 60, 42, 255))
        d.polygon([(sx - 15 * s, sy - 45 * s), (sx, sy - 80 * s), (sx + 15 * s, sy - 45 * s)], fill=(78, 60, 42, 255))

    # patas
    for k, ph in ((-0.28, 0), (0.28, math.pi)):
        lx = x + k * bw
        off = math.sin(run * 6.28 + ph) * 22 * s
        d.line([(lx, by + bh * 0.8), (lx + off, ground)], fill=color, width=int(20 * s))
    # cuerpo
    d.ellipse([bx, by, bx + bw, by + bh], fill=color)
    # cabeza
    hx, hy, hr = x + f * bw * 0.48, by - bh * 0.05, 40 * s
    d.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=color)
    # hocico
    d.polygon([(hx + f * hr * 0.5, hy - hr * 0.3), (hx + f * hr * 1.6, hy + hr * 0.25), (hx + f * hr * 0.5, hy + hr * 0.6)], fill=color)
    # orejas
    if species == "rabbit":
        for k in (-0.35, 0.25):
            ex = hx + k * hr
            d.ellipse([ex - 13 * s, hy - hr * 3.2, ex + 13 * s, hy - hr * 0.5], fill=color)
    elif species == "mouse":
        for k in (-0.6, 0.35):
            ex = hx + k * hr
            d.ellipse([ex - 28 * s, hy - hr * 1.8, ex + 28 * s, hy - hr * 0.4], fill=color)
    else:
        for k in (-0.55, 0.35):
            ex = hx + k * hr
            d.polygon([(ex - 20 * s, hy - hr * 0.6), (ex, hy - hr * (1.9 if species == "fox" else 1.5)), (ex + 20 * s, hy - hr * 0.6)], fill=color)
    # ojos brillantes
    ey = hy - hr * 0.15
    ex = hx + f * hr * 0.45
    d.ellipse([ex - 6 * s, ey - 6 * s, ex + 6 * s, ey + 6 * s], fill=(255, 235, 170, 255))
def vignette(img, strength=0.55):
    v = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(v)
    for i in range(40):
        t = i / 40
        a = int(255 * strength * (1 - t) ** 2.2)
        d.rectangle([W * t * 0.25, H * t * 0.25, W - W * t * 0.25, H - H * t * 0.25], outline=a, width=int(W * 0.25 / 40) + 2)
    v = v.filter(ImageFilter.GaussianBlur(80))
    black = Image.new("RGBA", img.size, (0, 0, 0, 255))
    img.paste(black, (0, 0), v)

def finish(img, name):
    vignette(img)
    out = img.convert("RGB").resize((1920, 1080), Image.LANCZOS)
    out.save(f"{OUT}/{name}.png")

# 1. Noche: la banda se acerca a la casa del gigante dormido.
img = Image.new("RGBA", (W, H))
gradient(img, [(0, (8, 10, 32)), (0.55, (24, 28, 70)), (0.8, (52, 44, 92)), (1, (30, 26, 50))])
stars(img, 420, 1, H * 0.6)
glow(img, (W * 0.42, H * 0.17), 70 * S, (200, 210, 255), 0.35)
d = ImageDraw.Draw(img)
d.ellipse([W * 0.42 - 70 * S, H * 0.17 - 70 * S, W * 0.42 + 70 * S, H * 0.17 + 70 * S], fill=(245, 240, 215))
d.ellipse([W * 0.42 - 40 * S, H * 0.17 - 85 * S, W * 0.42 + 100 * S, H * 0.17 + 55 * S], fill=(14, 17, 46))
hill(img, H * 0.66, 40 * S, (18, 22, 46), 2, 2)
(lx, ly), (lw, lh) = house(img, W * 0.68, H * 0.71, 0.85 * S, (26, 26, 48), (14, 14, 30), (255, 196, 110))
glow(img, (lx + lw / 2, ly + lh / 2), 90 * S, (255, 180, 90), 0.4)
house(img, W * 0.68, H * 0.71, 0.85 * S, (26, 26, 48), (14, 14, 30), (255, 196, 110))
zzz(img, lx + lw * 1.1, ly - 10 * S, 0.9 * S, (255, 220, 160, 230))
hill(img, H * 0.76, 22 * S, (10, 13, 28), 3, 4)
van(img, W * 0.07, H * 0.8, 0.75 * S, (48, 56, 84), (16, 18, 30))
for i, (sp, sk) in enumerate([("raccoon", True), ("mouse", False), ("fox", False), ("rabbit", True)]):
    critter(img, W * (0.3 + i * 0.065), H * 0.8, 0.5 * S, sp, (5, 6, 14, 255), run=i * 0.3, sack=sk)
finish(img, "T_UI_Transicion_01")

# 2. Amanecer: huida hacia la camioneta.
img = Image.new("RGBA", (W, H))
gradient(img, [(0, (22, 26, 70)), (0.4, (80, 60, 120)), (0.7, (230, 120, 80)), (0.82, (255, 190, 110)), (1, (60, 40, 50))])
stars(img, 120, 4, H * 0.3)
glow(img, (W * 0.5, H * 0.69), 160 * S, (255, 170, 80), 0.6)
d = ImageDraw.Draw(img)
d.ellipse([W * 0.5 - 140 * S, H * 0.69 - 140 * S, W * 0.5 + 140 * S, H * 0.69 + 140 * S], fill=(255, 220, 150))
hill(img, H * 0.68, 30 * S, (60, 38, 52), 5, 2)
(lx, ly), (lw, lh) = house(img, W * 0.72, H * 0.72, 0.8 * S, (48, 30, 44), (30, 18, 30), (255, 230, 170), sleeping=False)
hill(img, H * 0.78, 18 * S, (34, 20, 32), 6, 4)
van(img, W * 0.06, H * 0.8, 0.75 * S, (90, 70, 90), (30, 20, 30))
for i, sp in enumerate(["fox", "raccoon", "rabbit", "mouse"]):
    critter(img, W * (0.5 - i * 0.065), H * 0.8, 0.5 * S, sp, (20, 10, 18, 255), flip=True, run=i * 0.27, sack=(i == 1))
finish(img, "T_UI_Transicion_02")

# 3. La Madriguera: refugio bajo tierra con la banda alrededor del mapa.
img = Image.new("RGBA", (W, H))
gradient(img, [(0, (34, 22, 16)), (0.5, (60, 38, 24)), (1, (26, 16, 12))])
d = ImageDraw.Draw(img)
# boca de la madriguera (cueva redonda)
d.ellipse([W * 0.12, H * 0.08, W * 0.88, H * 1.25], fill=(78, 52, 34))
d.ellipse([W * 0.16, H * 0.14, W * 0.84, H * 1.2], fill=(96, 66, 42))
glow(img, (W * 0.5, H * 0.38), 120 * S, (255, 190, 100), 0.55)
d = ImageDraw.Draw(img)
# raíces
rnd = random.Random(9)
for _ in range(14):
    x = rnd.uniform(W * 0.15, W * 0.85)
    pts = [(x, 0)]
    for k in range(6):
        pts.append((pts[-1][0] + rnd.uniform(-60, 60) * S, pts[-1][1] + rnd.uniform(30, 60) * S))
    d.line(pts, fill=(40, 26, 18), width=int(rnd.uniform(6, 14) * S))
# lámpara colgante
d.line([(W * 0.5, H * 0.1), (W * 0.5, H * 0.33)], fill=(30, 20, 14), width=6 * S)
d.ellipse([W * 0.5 - 30 * S, H * 0.33, W * 0.5 + 30 * S, H * 0.33 + 60 * S], fill=(255, 220, 140))
# mesa con el plano de la casa
d.rounded_rectangle([W * 0.3, H * 0.58, W * 0.7, H * 0.63], radius=12 * S, fill=(50, 32, 20))
d.rectangle([W * 0.34, H * 0.63, W * 0.36, H * 0.78], fill=(50, 32, 20))
d.rectangle([W * 0.64, H * 0.63, W * 0.66, H * 0.78], fill=(50, 32, 20))
d.polygon([(W * 0.38, H * 0.585), (W * 0.62, H * 0.585), (W * 0.6, H * 0.56), (W * 0.4, H * 0.56)], fill=(220, 200, 160))
d.line([(W * 0.43, H * 0.572), (W * 0.5, H * 0.568), (W * 0.56, H * 0.576)], fill=(190, 60, 50), width=4 * S)
d.rectangle([0, H * 0.78, W, H], fill=(40, 26, 18))
for i, (sp, x, fl) in enumerate([("raccoon", 0.24, False), ("mouse", 0.38, False), ("rabbit", 0.62, True), ("fox", 0.77, True)]):
    critter(img, W * x, H * 0.785, 0.55 * S, sp, (22, 14, 10, 255), flip=fl)
finish(img, "T_UI_Transicion_03")
print("ok")
