# SPDX-License-Identifier: GPL-2.0-or-later
"""Resolve an experimental bootstrap plan from this image's exported symbols."""
import json

# Host replacement of rtld callbacks. Unlike the reference port, do not skip
# SDK service initialization: unimplemented IPC must remain visible.
STAGES = ('_init_libc0', 'nnosInitialize', '_init_libc1',
          'nninitInitializeSdkModule', 'nninitStartup', '_init_libc2')


def plan(image):
    def exported(name):
        hit = image.exports.get(name)
        if hit is None:
            raise ValueError('Missing startup export: ' + name)
        address = image.symbol_address(*hit)
        if address % 4 or image.locate(address) is None:
            raise ValueError('Startup export outside executable code: ' + name)
        return address
    stages = [dict(name=name, address=exported(name),
                   main_thread_argument=name == 'nnosInitialize') for name in STAGES]
    for module in image.modules:
        for offset in module.dynamic.get('INIT', []):
            if not offset:
                continue
            address = module.base + offset
            if address % 4 or image.locate(address) is None:
                raise ValueError('Module initializer outside executable code')
            stages.append(dict(name=module.name + ' DT_INIT', address=address,
                               main_thread_argument=False))
    stages.append(dict(name='nnMain', address=exported('nnMain'), main_thread_argument=False))
    return dict(version=1, experimental=True, stages=stages)


def header(manifest):
    stages = manifest['stages']
    rows = ['{%s, 0x%xULL, %d}' % (json.dumps(s['name']), s['address'],
                                  s['main_thread_argument']) for s in stages]
    return ('/* Image-derived experimental startup sequence. */\n'
            'static const struct { const char *name; uint64_t address; int thread_argument; }\n'
            'hos_startup[] = {\n' + ',\n'.join(rows) + '\n};\n'
            '#define HOS_STARTUP_COUNT %du\n' % len(rows))
