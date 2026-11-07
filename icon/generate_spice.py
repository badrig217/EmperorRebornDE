# Generates the Emperor Spice launcher art: the banner across the top of the launcher
# (launcher-header.bmp) and the application icon (EmperorSpice.ico).
# Original artwork drawn in code (a glowing spice sun over dunes) - no EA/Westwood art is used.
# Run: python icon/generate_spice.py   (needs Pillow)

import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = os.path.dirname(os.path.abspath(__file__))

BG_TOP = (30, 20, 16)
BG_BOT = (12, 9, 8)
SPICE = (255, 140, 40)
SPICE_HOT = (255, 214, 120)
DUNE_FAR = (120, 58, 26)
DUNE_NEAR = (70, 32, 16)
TEXT = (250, 204, 130)
SUBTEXT = (214, 196, 170)


def font(names, size):
    for n in names:
        for base in ("C:/Windows/Fonts/", "/usr/share/fonts/truetype/dejavu/", ""):
            try:
                return ImageFont.truetype(base + n, size)
            except OSError:
                pass
    return ImageFont.load_default()


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def emblem(size):
    """Round emblem: spice sun with a soft glow setting behind two dune ridges, gold rim."""
    s = size * 4  # supersample
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, s - 1, s - 1), fill=255)

    sky = Image.new("RGBA", (s, s))
    d = ImageDraw.Draw(sky)
    for y in range(s):
        d.line((0, y, s, y), fill=lerp((60, 26, 18), (240, 120, 40), y / s) + (255,))

    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, r = s * 0.5, s * 0.58, s * 0.2
    gd.ellipse((cx - r * 2.2, cy - r * 2.2, cx + r * 2.2, cy + r * 2.2), fill=SPICE + (120,))
    glow = glow.filter(ImageFilter.GaussianBlur(s * 0.08))
    sky.alpha_composite(glow)
    ImageDraw.Draw(sky).ellipse((cx - r, cy - r, cx + r, cy + r), fill=SPICE_HOT + (255,))

    dd = ImageDraw.Draw(sky)
    far = [(0, s * 0.70), (s * 0.25, s * 0.62), (s * 0.55, s * 0.70), (s * 0.80, s * 0.60), (s, s * 0.66), (s, s), (0, s)]
    near = [(0, s * 0.80), (s * 0.35, s * 0.72), (s * 0.70, s * 0.82), (s, s * 0.76), (s, s), (0, s)]
    dd.polygon(far, fill=DUNE_FAR + (255,))
    dd.polygon(near, fill=DUNE_NEAR + (255,))

    img.paste(sky, (0, 0), mask)
    ImageDraw.Draw(img).ellipse((s * 0.01, s * 0.01, s * 0.99, s * 0.99), outline=TEXT + (255,), width=int(s * 0.035))
    return img.resize((size, size), Image.LANCZOS)


def banner(sc=2):
    """Banner at 2x the 514x118 layout size; the launcher scales it down smoothly for the display DPI."""
    w, h = 514 * sc, 118 * sc
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        d.line((0, y, w, y), fill=lerp(BG_TOP, BG_BOT, y / h))
    haze = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(haze).ellipse((-20 * sc, -10 * sc, 140 * sc, 128 * sc), fill=SPICE + (45,))
    img = Image.alpha_composite(img.convert("RGBA"), haze.filter(ImageFilter.GaussianBlur(30 * sc))).convert("RGB")

    e = emblem(96 * sc)
    img.paste(e, (14 * sc, 11 * sc), e)

    title = font(["DejaVuSansCondensed-Bold.ttf", "impact.ttf"], 40 * sc)
    sub = font(["segoeui.ttf", "DejaVuSans.ttf"], 15 * sc)
    d = ImageDraw.Draw(img)
    d.text((124 * sc, 16 * sc), "EMPEROR SPICE", font=title, fill=TEXT)
    d.text((126 * sc, 72 * sc), "Emperor auf modernen PCs  \u00b7  Community-Edition", font=sub, fill=SUBTEXT)
    d.text((126 * sc, 92 * sc), "based on / basiert auf Emperor Reborn", font=sub, fill=(150, 136, 118))
    img.save(os.path.join(OUT, "launcher-header.bmp"))
    img.resize((514, 118), Image.LANCZOS).save(os.path.join(OUT, "launcher-header-preview.png"))


def icon():
    big = emblem(256)
    big.save(os.path.join(OUT, "EmperorSpice.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])


if __name__ == "__main__":
    banner()
    icon()
    print("done")
