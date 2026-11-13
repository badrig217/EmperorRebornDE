"""Tiny helpers for looking at Game.exe (v1.09, MD5 7dc4fda2f6b7e8a6b70e846686a81938) in Python.

    GAME_EXE=/path/to/Game.exe python3 -i tools/re/re_tools.py
    >>> hex(va_of_str('MaxZoom')); [hex(x) for x in xrefs(va_of_str('MaxZoom'))]; dis(0x532e07, 0x70)

Needs: pip install pefile capstone
"""
import os, struct
import pefile, capstone

pe = pefile.PE(os.environ.get('GAME_EXE', 'Game.exe'))
img = pe.get_memory_mapped_image()
base = 0x400000
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def va_of_str(s):
    """Virtual address of a NUL-terminated ASCII string, or None."""
    i = img.find(s.encode() + b'\0')
    return base + i if i >= 0 else None


def xrefs(va):
    """All places where the 32-bit value va appears (pushes, movs, tables...)."""
    pat = struct.pack('<I', va)
    out, i = [], 0
    while True:
        i = img.find(pat, i)
        if i < 0:
            return out
        out.append(base + i)
        i += 1


def dis(va, n=0x80):
    """Disassemble n bytes at va. Start on an instruction boundary or capstone stops early."""
    for ins in md.disasm(img[va - base:va - base + n], va):
        print(hex(ins.address), ins.mnemonic, ins.op_str)
