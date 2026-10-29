"""Inspect NSO files in an ExeFS ZIP without extracting paths or running guest code."""
import argparse
import json
import pathlib
import zipfile
from nso import load_nso, MAX_SEGMENT


def analyze(path):
    modules = {}
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            if entry.is_dir():
                continue
            with archive.open(entry) as stream:
                magic = stream.read(4)
            if magic != b'NSO0':
                continue
            if entry.file_size > MAX_SEGMENT:
                raise ValueError('NSO input exceeds size limit')
            module = load_nso(archive.read(entry))
            for segment in module['segments']:
                segment.pop('payload')
            modules[entry.filename] = module
    if not modules:
        raise ValueError('No NSO0 modules found')
    return dict(status='analysis only; game execution not implemented', modules=modules)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exefs_zip', type=pathlib.Path)
    args = parser.parse_args()
    try:
        print(json.dumps(analyze(args.exefs_zip), indent=2))
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        parser.exit(1, f'{error}\n')
