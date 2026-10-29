"""Translate a selected NSO .text block; does not discover game functions."""
import argparse
import pathlib
from nso import load_nso
from recompile import translate


def extract(raw, address, size):
    module = load_nso(raw)
    text = module['segments'][0]
    relative = address - text['address']
    if address % 4 or size <= 0 or size % 4 or relative < 0 or relative + size > text['size']:
        raise ValueError('Block must be aligned and contained in .text')
    return text['payload'][relative:relative + size]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nso', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--address', required=True, type=lambda x: int(x, 0), help='Module-relative virtual address')
    parser.add_argument('--size', required=True, type=lambda x: int(x, 0))
    args = parser.parse_args()
    try:
        source = translate(extract(args.nso.read_bytes(), args.address, args.size))
        args.output.write_text(source, encoding='utf-8')
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')
