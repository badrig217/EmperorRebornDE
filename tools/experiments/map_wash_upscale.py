"""PoC: CPT wash -> 2x upscaled test.tga (4096x4096, 32-bit)."""
import sys, os, numpy as np, torch
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 're'))
from map_wash_decode import decode_cpt
from spandrel import ModelLoader
torch.set_num_threads(2)
m = ModelLoader().load_from_file(os.environ.get('ESRGAN_MODEL', 'RealESRGAN_x4plus.pth')).model.eval()

def esr4(rgb, tile=256, pad=16):
    h, w, _ = rgb.shape
    out = np.zeros((h*4, w*4, 3), np.float32)
    p = np.pad(rgb, ((pad, pad), (pad, pad), (0, 0)), mode='edge').astype(np.float32)/255
    for y in range(0, h, tile):
        for x in range(0, w, tile):
            t = p[y:y+tile+2*pad, x:x+tile+2*pad]
            with torch.no_grad():
                r = m(torch.from_numpy(t).permute(2,0,1)[None]).clamp(0,1)[0].permute(1,2,0).numpy()
            th, tw = min(tile, h-y), min(tile, w-x)
            out[y*4:(y+th)*4, x*4:(x+tw)*4] = r[pad*4:(pad+th)*4, pad*4:(pad+tw)*4]
            print('.', end='', flush=True)
    return out

def up2(rgb):
    big = esr4(rgb)
    h, w, _ = rgb.shape
    return np.asarray(Image.fromarray((big*255+.5).astype(np.uint8)).resize((w*2, h*2), Image.LANCZOS))

mapdir, outdir = sys.argv[1], sys.argv[2]
cells = dict(l.split() for l in open(os.path.join(mapdir, 'map.inf')) if l.strip())
W, H = int(cells['WIDTH'])*8, int(cells['HEIGHT'])*8
full = decode_cpt(os.path.join(mapdir, 'test.CPT'))
W2 = min(2048, (W+255)//256*256); H2 = min(2048, (H+255)//256*256)
reg = full[:H2, :W2]
rgb = up2(reg[:, :, :3]); print(' rgb done')
a = up2(np.repeat(reg[:, :, 3:4], 3, 2)).mean(2)
a = a.clip(0, 255)
out = np.zeros((4096, 4096, 4), np.uint8)
out[:H2*2, :W2*2, :3] = rgb; out[:H2*2, :W2*2, 3] = a
os.makedirs(outdir, exist_ok=True)
# TGA 32-bit, top-left origin (desc 0x28)
hdr = bytes([0,0,2]) + bytes(9) + (4096).to_bytes(2,'little')*2 + bytes([32, 0x28])
open(os.path.join(outdir, 'test.tga'), 'wb').write(hdr + out[:, :, [2,1,0,3]].tobytes())
Image.fromarray(out[:H2*2, :W2*2, :3]).save(os.path.join(outdir, 'preview_rgb.png'))
Image.fromarray(full[:H2, :W2, :3]).save(os.path.join(outdir, 'orig_rgb.png'))
print('ok', W, H)
