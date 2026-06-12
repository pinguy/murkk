#!/usr/bin/env python3
"""Tiny ELF64 little-endian sstrip fallback.
Removes section headers and truncates to the last loadable segment.
Good enough for normal dynamically-linked x86_64 Linux binaries.
"""
import os, struct, sys
PT_LOAD = 1
ELF_HDR = '<16sHHIQQQIHHHHHH'
PHDR = '<IIQQQQQQ'

def die(msg):
    print('sstrip64:', msg, file=sys.stderr); sys.exit(1)

def main(path):
    with open(path, 'r+b') as f:
        data = bytearray(f.read())
        if len(data) < 64 or data[:4] != b'\x7fELF': die('not an ELF file')
        if data[4] != 2 or data[5] != 1: die('only ELF64 little-endian is supported')
        fields = list(struct.unpack_from(ELF_HDR, data, 0))
        e_phoff, e_phentsize, e_phnum = fields[5], fields[9], fields[10]
        if e_phoff == 0 or e_phnum == 0: die('no program headers')
        keep = 0
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            if off + 56 > len(data): die('program header outside file')
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(PHDR, data, off)
            if p_type == PT_LOAD:
                keep = max(keep, p_offset + p_filesz)
        if keep == 0: die('no PT_LOAD segments')
        # Section table is not needed at runtime.
        fields[6] = 0      # e_shoff
        fields[11] = 0     # e_shentsize
        fields[12] = 0     # e_shnum
        fields[13] = 0     # e_shstrndx
        struct.pack_into(ELF_HDR, data, 0, *fields)
        f.seek(0); f.write(data[:keep]); f.truncate(keep)

if __name__ == '__main__':
    if len(sys.argv) != 2: die('usage: sstrip64.py FILE')
    main(sys.argv[1])
