"""AoC research tools: inspect ExeFS/assets and translate supported CPU blocks."""
import argparse
import collections
import json
from pathlib import Path, PurePosixPath
import zipfile
from analyze_exefs import analyze
from extract_block import extract
from recompile import translate
from build_romfs import build as build_romfs


def inspect_assets(path, verify=False):
    groups = collections.Counter()
    total = 0
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        for entry in entries:
            normalized = entry.filename.replace('\\', '/')
            name = PurePosixPath(normalized)
            if name.is_absolute() or '..' in name.parts or (name.parts and ':' in name.parts[0]):
                raise ValueError('Unsafe archive path')
            if entry.flag_bits & 1:
                raise ValueError('Encrypted ZIP entries are unsupported')
            if not entry.is_dir():
                groups[name.parts[0] if name.parts else 'unknown'] += 1
                total += entry.file_size
        if verify:
            failed = archive.testzip()
            if failed:
                raise ValueError('ZIP CRC mismatch: ' + failed)
    return dict(entries=len(entries), files_by_root=dict(groups),
                uncompressed_bytes=total, crc_verified=verify,
                status='Asset inventory only; game execution not implemented')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    exe = commands.add_parser('inspect-exefs')
    exe.add_argument('input', type=Path)
    assets = commands.add_parser('inspect-assets')
    assets.add_argument('input', type=Path)
    assets.add_argument('--verify-crc', action='store_true', help='Read every file; may take several minutes')
    romfs = commands.add_parser('build-romfs')
    romfs.add_argument('input', type=Path)
    romfs.add_argument('output', type=Path)
    romfs.add_argument('--prefix', default='romfs')
    block = commands.add_parser('translate-nso')
    block.add_argument('input', type=Path)
    block.add_argument('output', type=Path)
    block.add_argument('--address', required=True, type=lambda x: int(x, 0))
    block.add_argument('--size', required=True, type=lambda x: int(x, 0))
    args = parser.parse_args()
    try:
        if args.command == 'inspect-exefs':
            print(json.dumps(analyze(args.input), indent=2))
        elif args.command == 'build-romfs':
            print(json.dumps(build_romfs(args.input, args.output, args.prefix), indent=2))
        elif args.command == 'inspect-assets':
            print(json.dumps(inspect_assets(args.input, args.verify_crc), indent=2))
        else:
            source = translate(extract(args.input.read_bytes(), args.address, args.size))
            args.output.write_text(source, encoding='utf-8')
    except (ValueError, OSError, zipfile.BadZipFile, RuntimeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()

