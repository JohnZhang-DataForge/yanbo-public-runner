"""Experimental raw A64 block -> portable C++17 translator. Not a game port."""
import argparse
import pathlib
import struct


def translate(data):
    if not data or len(data) % 4:
        raise ValueError('Input must be nonempty, aligned raw little-endian A64 code')
    lines = []
    for offset in range(0, len(data), 4):
        word, = struct.unpack_from('<I', data, offset)
        rd, rn = word & 31, (word >> 5) & 31
        width = 64 if word >> 31 else 32
        cast = f'uint{width}_t'
        if word == 0xD503201F:
            statement = '; // NOP'
        elif word & 0x7F800000 == 0x52800000:  # MOVZ
            shift = ((word >> 21) & 3) * 16
            if shift >= width:
                raise ValueError(f'Invalid MOVZ at 0x{offset:x}')
            value = ((word >> 5) & 65535) << shift
            statement = '; // zero-register write' if rd == 31 else f's.x[{rd}] = {cast}({value}ULL);'
        elif word & 0x1F800000 == 0x11000000 and not word & 0x20000000:
            # ADD/SUB immediate without flags; register 31 means SP.
            imm = ((word >> 10) & 4095) << (12 if word & (1 << 22) else 0)
            src = 's.sp' if rn == 31 else f's.x[{rn}]'
            dst = 's.sp' if rd == 31 else f's.x[{rd}]'
            op = '-' if word & 0x40000000 else '+'
            statement = f'{dst} = {cast}({src} {op} {imm}ULL);'
        elif word & 0xFFFFFC1F == 0xD65F0000:
            if offset != len(data) - 4:
                raise ValueError('Prototype accepts one block ending in RET only')
            src = '0ULL' if rn == 31 else f's.x[{rn}]'
            statement = f's.pc = {src}; return;'
        else:
            raise ValueError(f'Unsupported instruction 0x{word:08x} at 0x{offset:x}; no output generated')
        lines.append(f'  // guest +0x{offset:x}: {word:08x}\n  {statement}')
    if (word & 0xFFFFFC1F) != 0xD65F0000:
        raise ValueError('Block must end in RET')
    return '#pragma once\n#include <cstdint>\nstruct GuestState { uint64_t x[31]{}; uint64_t sp{}, pc{}; };\ninline void run_block(GuestState& s) {\n' + '\n'.join(lines) + '\n}\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = translate(args.input.read_bytes())
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')
    args.output.write_text(result, encoding='utf-8')


if __name__ == '__main__':
    main()
