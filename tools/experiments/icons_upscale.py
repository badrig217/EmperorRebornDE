import sys, os, numpy as np, torch
from PIL import Image
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 're'))
from upscale import read_tga
from spandrel import ModelLoader
torch.set_num_threads(2)
m = ModelLoader().load_from_file(os.environ.get('ESRGAN_MODEL', 'RealESRGAN_x4plus.pth')).model.eval()
src, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
os.makedirs(out, exist_ok=True)
def esr(rgb):
    pad = 8
    p = np.pad(rgb, ((pad, pad), (pad, pad), (0, 0)), mode='reflect').astype(np.float32) / 255
    with torch.no_grad():
        r = m(torch.from_numpy(p).permute(2, 0, 1)[None]).clamp(0, 1)[0].permute(1, 2, 0).numpy()
    return r[pad*4:-pad*4, pad*4:-pad*4]
for n in names:
    a, *_ = read_tga(os.path.join(src, n))
    h, w = a.shape[:2]
    big = esr(a[..., :3])
    up = np.asarray(Image.fromarray((big * 255 + .5).astype(np.uint8)).resize((w * 2, h * 2), Image.LANCZOS))
    # keep the original colours: correct the average colour per 2x2 block back to the source pixel
    down = up.reshape(h, 2, w, 2, 3).mean((1, 3))
    up = np.clip(up + np.repeat(np.repeat(a[..., :3].astype(np.float32) - down, 2, 0), 2, 1) * 0.6, 0, 255).astype(np.uint8)
    hdr = bytes([0, 0, 2]) + bytes(9) + (w*2).to_bytes(2, 'little') + (h*2).to_bytes(2, 'little') + bytes([24, 0x20])
    open(os.path.join(out, n), 'wb').write(hdr + up[:, :, ::-1].tobytes())
    Image.fromarray(np.hstack([np.asarray(Image.fromarray(a[..., :3]).resize((w*2, h*2), Image.BILINEAR)), up])).save(os.path.join(out, n + '.cmp.png'))
    print(n, flush=True)
