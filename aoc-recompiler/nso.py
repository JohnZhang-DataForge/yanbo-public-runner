"""Bounded NSO0 segment loader. No keys or firmware required."""
import hashlib
import struct

MAX_SEGMENT = 256 * 1024 * 1024


def decompress_lz4(data, expected):
    if not 0 <= expected <= MAX_SEGMENT:
        raise ValueError('Segment exceeds size limit')
    out = bytearray()
    pos = 0

    def length(base):
        nonlocal pos
        if base == 15:
            while True:
                if pos >= len(data):
                    raise ValueError('Truncated LZ4 length')
                extra = data[pos]
                pos += 1
                base += extra
                if extra != 255:
                    break
        return base

    while pos < len(data):
        token = data[pos]
        pos += 1
        count = length(token >> 4)
        if pos + count > len(data) or len(out) + count > expected:
            raise ValueError('Invalid LZ4 literals')
        out.extend(data[pos:pos + count])
        pos += count
        if pos == len(data):
            break
        if pos + 2 > len(data):
            raise ValueError('Truncated LZ4 offset')
        distance = int.from_bytes(data[pos:pos + 2], 'little')
        pos += 2
        if distance == 0 or distance > len(out):
            raise ValueError('Invalid LZ4 offset')
        count = length(token & 15) + 4
        if len(out) + count > expected:
            raise ValueError('LZ4 output exceeds declared size')
        start = len(out) - distance
        while count:
            chunk = out[start:start + min(distance, count)]
            out.extend(chunk)
            count -= len(chunk)
            start += len(chunk)
    if len(out) != expected:
        raise ValueError('LZ4 size mismatch')
    return bytes(out)


def load_nso(raw):
    if len(raw) < 256 or raw[:4] != b'NSO0':
        raise ValueError('Expected NSO0 header')
    flags, = struct.unpack_from('<I', raw, 12)
    if flags & ~0x3f:
        raise ValueError('Unsupported NSO flags')
    segments = []
    for index, name in enumerate(('text', 'rodata', 'data')):
        offset, address, size = struct.unpack_from('<III', raw, 16 + index * 16)
        compressed = bool(flags & (1 << index))
        stored = struct.unpack_from('<I', raw, 96 + index * 4)[0] if compressed else size
        if size > MAX_SEGMENT or offset < 256 or offset + stored > len(raw):
            raise ValueError(f'Invalid {name} bounds')
        for previous in segments:
            if size and previous['size'] and max(address, previous['address']) < min(address + size, previous['address'] + previous['size']):
                raise ValueError('Overlapping memory segments')
        payload = raw[offset:offset + stored]
        payload = decompress_lz4(payload, size) if compressed else payload
        digest = hashlib.sha256(payload).digest()
        checked = bool(flags & (1 << (index + 3)))
        if checked and digest != raw[160 + 32 * index:192 + 32 * index]:
            raise ValueError(f'{name} SHA-256 mismatch')
        segments.append(dict(name=name, address=address, size=size, stored_size=stored,
                             compressed=compressed, hash_verified=checked,
                             sha256=digest.hex(), payload=payload))
    return dict(build_id=raw[64:96].hex(), bss_size=struct.unpack_from('<I', raw, 60)[0], segments=segments)
