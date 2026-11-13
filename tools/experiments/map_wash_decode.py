import numpy as np, sys
from PIL import Image
def c565(c):
    r=((c>>11)&31)*255/31; g=((c>>5)&63)*255/63; b=(c&31)*255/31
    return np.stack([r,g,b],-1)
def decode_cpt(path):
    raw=np.frombuffer(open(path,"rb").read()[8:],dtype=np.uint8).reshape(-1,16)
    al=raw[:,:8]; a=np.stack([al&15,al>>4],-1).reshape(-1,16)*17
    c0=raw[:,8].astype(np.uint32)|raw[:,9].astype(np.uint32)<<8
    c1=raw[:,10].astype(np.uint32)|raw[:,11].astype(np.uint32)<<8
    idx=np.frombuffer(raw[:,12:16].tobytes(),dtype='<u4')
    C0=c565(c0); C1=c565(c1)
    pal=np.stack([C0,C1,(2*C0+C1)/3,(C0+2*C1)/3],1)  # DXT3 always 4-color
    sel=np.stack([(idx>>(2*i))&3 for i in range(16)],1)
    rgb=np.take_along_axis(pal,sel[:,:,None].repeat(3,2),1)
    rgba=np.concatenate([rgb,a[:,:,None]],2)  # (N,16,4)
    # strip of 64 chunks 256x256, chunk k -> grid (k//8,k%8)
    img=rgba.reshape(64,64,64,4,4,4).transpose(0,1,3,2,4,5).reshape(64,256,256,4)
    full=img.reshape(8,8,256,256,4).transpose(0,2,1,3,4).reshape(2048,2048,4)
    return full.astype(np.uint8)
if __name__=="__main__":
    f=decode_cpt(sys.argv[1])
    Image.fromarray(f[:,:,:3]).save(sys.argv[2]+"_rgb.png")
    Image.fromarray(f[:,:,3]).save(sys.argv[2]+"_a.png")
