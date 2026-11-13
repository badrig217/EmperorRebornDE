"""Upscale Emperor TGA textures with Real-ESRGAN, keeping the original TGA layout.

usage: python3 upscale.py SRC_TEXTURE_DIR OUT_TEXTURE_DIR SCALE NAME [NAME ...]
SCALE: 2 or 4 (model runs at 4x, result is downsampled for 2x)
"""
import os, sys
import numpy as np
import torch
from PIL import Image
from spandrel import ModelLoader

MODEL = os.environ.get('ESRGAN_MODEL', 'RealESRGAN_x4plus.pth')  # download from the Real-ESRGAN releases
PAD = 8
_model = None


def model():
    global _model
    if _model is None:
        _model = ModelLoader().load_from_file(MODEL).model.eval()
    return _model


def read_tga(path):
    d = open(path, 'rb').read()
    idlen, cmtype, itype = d[0], d[1], d[2]
    w, h, bpp, desc = int.from_bytes(d[12:14], 'little'), int.from_bytes(d[14:16], 'little'), d[16], d[17]
    assert itype == 2 and cmtype == 0, f'unsupported TGA type {itype}'
    off = 18 + idlen
    n = w * h
    if bpp == 24:
        a = np.frombuffer(d, np.uint8, n * 3, off).reshape(h, w, 3)[..., ::-1]
        rgba = np.dstack([a, np.full((h, w), 255, np.uint8)])
    elif bpp == 32:
        a = np.frombuffer(d, np.uint8, n * 4, off).reshape(h, w, 4)
        rgba = a[..., [2, 1, 0, 3]]
    elif bpp == 16:
        v = np.frombuffer(d, '<u2', n, off).reshape(h, w).astype(np.uint32)
        r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
        al = np.where(v & 0x8000, 255, 0) if desc & 0x0F else np.full((h, w), 255)
        rgba = np.dstack([(c * 255 // 31) for c in (r, g, b)] + [al]).astype(np.uint8)
    else:
        raise ValueError(bpp)
    if not desc & 0x20:  # bottom-left origin -> flip to top-down
        rgba = rgba[::-1]
    return np.ascontiguousarray(rgba), bpp, desc


def write_tga(path, rgba, bpp, desc):
    h, w = rgba.shape[:2]
    px = rgba if desc & 0x20 else rgba[::-1]
    if bpp == 24:
        body = px[..., [2, 1, 0]].tobytes()
    elif bpp == 32:
        body = px[..., [2, 1, 0, 3]].tobytes()
    else:  # 16-bit ARGB1555
        p = px.astype(np.uint32)
        v = ((p[..., 0] * 31 + 127) // 255 << 10) | ((p[..., 1] * 31 + 127) // 255 << 5) | ((p[..., 2] * 31 + 127) // 255)
        if desc & 0x0F:
            v = v | np.where(p[..., 3] >= 128, 0x8000, 0).astype(np.uint32)
        body = v.astype('<u2').tobytes()
    hdr = bytes([0, 0, 2]) + bytes(9) + w.to_bytes(2, 'little') + h.to_bytes(2, 'little') + bytes([bpp, desc])
    footer = bytes(8) + b'TRUEVISION-XFILE.\x00'
    open(path, 'wb').write(hdr + body + footer)


@torch.inference_mode()
def esrgan4(rgb):
    h, w = rgb.shape[:2]
    p = np.pad(rgb, ((PAD, PAD), (PAD, PAD), (0, 0)), mode='wrap')  # textures tile -> wrap padding avoids seams
    t = torch.from_numpy(p).permute(2, 0, 1)[None].float() / 255
    out = model()(t)[0].clamp(0, 1).permute(1, 2, 0).numpy()
    out = (out * 255 + 0.5).astype(np.uint8)
    return out[PAD * 4:PAD * 4 + h * 4, PAD * 4:PAD * 4 + w * 4]


def key_mask(rgb):
    """Pure magenta is Emperor's colour key (transparent)."""
    r, g, b = rgb[..., 0].astype(int), rgb[..., 1].astype(int), rgb[..., 2].astype(int)
    return (r >= 248) & (g <= 8) & (b >= 248)


def inpaint(rgb, mask):
    """Fill keyed pixels from their neighbours so magenta does not bleed into the upscale."""
    if not mask.any() or mask.all():
        return rgb
    img = rgb.astype(np.float32)
    known = ~mask
    img[mask] = 0
    while not known.all():
        acc = np.zeros_like(img)
        cnt = np.zeros(mask.shape, np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            k = np.roll(known, (dy, dx), (0, 1))
            acc += np.roll(img, (dy, dx), (0, 1)) * k[..., None]
            cnt += k
        new = (~known) & (cnt > 0)
        img[new] = acc[new] / cnt[new][:, None]
        known = known | new
    return img.astype(np.uint8)


def resize(a, size):
    return np.array(Image.fromarray(a).resize(size, Image.BICUBIC))


def back_project(big, small, iters=3):
    """Correct colour drift: make the downscaled result match the original exactly on average."""
    h, w = small.shape[:2]
    H, W = big.shape[:2]
    out = big.astype(np.float32)
    for _ in range(iters):
        down = out.reshape(h, H // h, w, W // w, 3).mean((1, 3))
        err = small.astype(np.float32) - down
        up = np.stack([np.array(Image.fromarray(err[..., c]).resize((W, H), Image.BICUBIC)) for c in range(3)], -1)
        out = np.clip(out + up, 0, 255)
    return (out + 0.5).astype(np.uint8)


def upscale(src, dst, scale):
    rgba, bpp, desc = read_tga(src)
    h, w = rgba.shape[:2]
    rgb = rgba[..., :3]
    mask = key_mask(rgb)
    filled = inpaint(rgb, mask)
    big = esrgan4(filled)
    big = back_project(big, filled)
    size = (w * scale, h * scale)
    if scale != 4:
        big = resize(big, size)
    alpha = np.array(Image.fromarray(rgba[..., 3]).resize(size, Image.BICUBIC))
    if mask.any():
        m = np.array(Image.fromarray(mask.astype(np.uint8) * 255).resize(size, Image.BILINEAR)) >= 128
        big[m] = (255, 0, 255)
        # keep near-key pixels clearly away from the key colour
        bad = key_mask(big) & ~m
        big[bad] = filled.mean((0, 1)).astype(np.uint8)
    write_tga(dst, np.dstack([big, alpha]), bpp, desc)
    return (w, h), size


if __name__ == '__main__':
    srcdir, outdir, scale = sys.argv[1], sys.argv[2], int(sys.argv[3])
    os.makedirs(outdir, exist_ok=True)
    torch.set_num_threads(os.cpu_count() or 4)
    for name in sys.argv[4:]:
        a, b = upscale(os.path.join(srcdir, name), os.path.join(outdir, name), scale)
        print(f'{name}: {a} -> {b}', flush=True)
