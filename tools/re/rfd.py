"""Emperor: Battle for Dune RFH/RFD archive tool.

RFH (index), per entry, little-endian:
  u32 name_len (incl. NUL), u32 timestamp, u32 flags (2 = zlib-compressed),
  u32 stored_size (bytes in RFD incl. 4-byte prefix), u32 raw_size,
  u32 offset_in_rfd, char name[name_len]
RFD (data), per entry at offset:
  u32 raw_size, zlib stream

usage:
  python3 rfd.py unpack ARCHIVE_BASE OUTDIR     (ARCHIVE_BASE without .RFH/.RFD)
  python3 rfd.py pack   OUTDIR ARCHIVE_BASE     (uses OUTDIR/_index.txt for order/flags)
"""
import os, struct, sys, zlib


def read_index(path):
    d = open(path, 'rb').read()
    pos, entries = 0, []
    while pos < len(d):
        nl, ts, flags, stored, raw, off = struct.unpack_from('<6I', d, pos)
        pos += 24
        name = d[pos:pos + nl].split(b'\0')[0].decode('latin1')
        pos += nl
        entries.append(dict(name=name, ts=ts, flags=flags, stored=stored, raw=raw, off=off))
    return entries


def unpack(base, outdir):
    entries = read_index(base + '.RFH')
    data = open(base + '.RFD', 'rb').read()
    with open(os.path.join(outdir, '_index.txt'), 'w', encoding='latin1') as idx:
        for e in entries:
            blob = data[e['off']:e['off'] + e['stored']]
            if e['flags'] & 2:
                assert struct.unpack_from('<I', blob)[0] == e['raw']
                content = zlib.decompress(blob[4:])
            else:
                content = blob
            assert len(content) == e['raw'], e['name']
            dest = os.path.join(outdir, *e['name'].split('\\'))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, 'wb').write(content)
            idx.write(f"{e['name']}\t{e['ts']}\t{e['flags']}\n")
            print(f"{e['name']}  {e['raw']} bytes")


def pack(indir, base):
    lines = open(os.path.join(indir, '_index.txt'), encoding='latin1').read().splitlines()
    rfh, rfd = bytearray(), bytearray()
    for line in lines:
        name, ts, flags = line.split('\t')
        ts, flags = int(ts), int(flags)
        content = open(os.path.join(indir, *name.split('\\')), 'rb').read()
        blob = struct.pack('<I', len(content)) + zlib.compress(content, 9) if flags & 2 else content
        nb = name.encode('latin1') + b'\0'
        rfh += struct.pack('<6I', len(nb), ts, flags, len(blob), len(content), len(rfd)) + nb
        rfd += blob
    open(base + '.RFH', 'wb').write(rfh)
    open(base + '.RFD', 'wb').write(rfd)


if __name__ == '__main__':
    cmd, a, b = sys.argv[1:4]
    os.makedirs(b, exist_ok=True) if cmd == 'unpack' else None
    unpack(a, b) if cmd == 'unpack' else pack(a, b)
