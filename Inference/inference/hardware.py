"""Small GGUF metadata reader and conservative context capacity estimate."""
import re
import struct


def metadata(path):
    formats = {0: 'B', 1: 'b', 2: 'H', 3: 'h', 4: 'I', 5: 'i', 6: 'f', 7: '?', 10: 'Q', 11: 'q', 12: 'd'}
    with path.open('rb') as f:
        def number(fmt):
            return struct.unpack('<' + fmt, f.read(struct.calcsize(fmt)))[0]
        def string(keep=True):
            size = number('Q')
            if size > 64 * 1024 * 1024:
                raise ValueError('Invalid GGUF string length')
            if keep:
                return f.read(size).decode('utf-8')
            f.seek(size, 1)
        def value(kind, keep=True):
            if kind == 8:
                return string(keep)
            if kind == 9:
                subtype, length = number('I'), number('Q')
                if subtype in formats:
                    f.seek(struct.calcsize(formats[subtype]) * length, 1)
                else:
                    if length > 2000000:
                        raise ValueError('Invalid GGUF array length')
                    for _ in range(length):
                        value(subtype, False)
                return None
            if kind not in formats:
                raise ValueError('Unsupported GGUF metadata type')
            return number(formats[kind])
        if f.read(4) != b'GGUF' or number('I') not in (2, 3):
            raise ValueError('Expected GGUF v2/v3')
        number('Q')
        count = number('Q')
        if count > 100000:
            raise ValueError('Invalid GGUF metadata count')
        result = {}
        for _ in range(count):
            key, kind = string(), number('I')
            keep = not key.startswith('tokenizer.')
            entry = value(kind, keep)
            if keep and kind != 9:
                result[key] = entry
        return result


def choose_context(info, model_bytes, devices, device, kv_cache, reserve_mib, parallel):
    arch = info.get('general.architecture', '')
    try:
        layers = info[arch + '.block_count']
        heads = info[arch + '.attention.head_count_kv']
        key = info.get(arch + '.attention.key_length') or info[arch + '.embedding_length'] // info[arch + '.attention.head_count']
        val = info.get(arch + '.attention.value_length', key)
        kv_per_token = layers * heads * (key + val) * (34 / 32 if kv_cache == 'q8_0' else 2)
        match = re.search(rf'{re.escape(device)}:.*?([\d]+) MiB free', devices)
        if not match or kv_per_token <= 0:
            return 4096
        # Leave compute/driver room beyond the user reserve and on-disk weight size.
        spare = (int(match[1]) - reserve_mib - 768) * 1024**2 - model_bytes
        capacity = int(spare / kv_per_token / parallel)
        ceiling = min(32768, int(info.get(arch + '.context_length', 32768)))
        return max([n for n in (4096, 8192, 16384, 32768) if n <= capacity and n <= ceiling] or [4096])
    except (KeyError, TypeError, ZeroDivisionError, ValueError):
        return 4096
