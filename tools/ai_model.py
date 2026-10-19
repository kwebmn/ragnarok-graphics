"""Converts a Real-ESRGAN SRVGGNetCompact checkpoint (.pth) to the "SRVG" file src/core/ai/srvgg.cpp reads.

Without torch: a restricted unpickler that only builds tensor descriptions (no code from the file runs), then the
weights as little-endian float32 in our order. Usage: python3 -I tools/ai_model.py realesr-general-x4v3.pth out.bin
The models in dist/ai/ came from github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/: realesr-general-x4v3.pth,
realesr-general-wdn-x4v3.pth and realesr-animevideov3.pth (16 convolutions instead of 32; BSD-3-Clause,
dist/ai/LICENSE-Real-ESRGAN.txt)."""
import pickle, struct, sys, zipfile

class Storage:
    def __init__(self, key, numel, dtype='f'):
        self.key, self.numel, self.dtype = key, numel, dtype  # 'f' float32, 'e' float16 (2.5.27)

class Tensor:
    def __init__(self, storage, offset, size, stride):
        self.storage, self.offset, self.size, self.stride = storage, offset, tuple(size), tuple(stride)

def rebuild_tensor_v2(storage, offset, size, stride, *rest):
    return Tensor(storage, offset, size, stride)

def rebuild_tensor(storage, offset, size, stride):  # 2.5.27: torch before 1.6
    return Tensor(storage, offset, size, stride)

class OrderedDict(dict):
    pass

ALLOWED = {
    ('collections', 'OrderedDict'): OrderedDict,
    ('typing', 'OrderedDict'): OrderedDict,
    ('torch._utils', '_rebuild_tensor_v2'): rebuild_tensor_v2,
    ('torch._utils', '_rebuild_tensor'): rebuild_tensor,
    ('torch', 'FloatStorage'): 'f',
    ('torch', 'HalfStorage'): 'e',
}

class Safe(pickle.Unpickler):
    def find_class(self, module, name):
        if (module, name) in ALLOWED:
            return ALLOWED[(module, name)]
        raise pickle.UnpicklingError(f'refused {module}.{name}')
    def __init__(self, f):
        super().__init__(f)
        self.storages = {}
    def persistent_load(self, pid):
        kind, storage_type, key, location, numel = pid[:5]
        if kind != 'storage' or storage_type not in ('f', 'e'):
            raise pickle.UnpicklingError(f'refused storage {pid!r}')
        if key not in self.storages:
            self.storages[key] = Storage(key, numel, storage_type)
        return self.storages[key]

def load(path):
    """2.5.27: the zip of torch 1.6 and later, or the older plain file (magic, protocol, system's info, the object,
    its storages' keys, then each storage's count and data); float32 or float16 storages."""
    if zipfile.is_zipfile(path):
        z = zipfile.ZipFile(path)
        names = z.namelist()
        pkl = [n for n in names if n.endswith('/data.pkl')][0]
        root = pkl[:-len('data.pkl')]
        u = Safe(z.open(pkl))
        obj = u.load()
        raw = {key: z.read(root + 'data/' + key) for key in u.storages}
    else:
        with open(path, 'rb') as f:
            for _ in range(3):
                Safe(f).load()  # plain numbers and a dict: nothing to build
            u = Safe(f)
            obj = u.load()
            keys = Safe(f).load()
            raw = {}
            for key in keys:
                s = u.storages[key]
                numel = struct.unpack('<q', f.read(8))[0]
                raw[key] = f.read(numel * (2 if s.dtype == 'e' else 4))
    for k in ('params_ema', 'params', 'state_dict', 'model'):
        if isinstance(obj, dict) and k in obj and isinstance(obj[k], dict):
            obj = obj[k]
            break
    out = {}
    for k, t in obj.items():
        if not isinstance(t, Tensor):
            continue
        n = 1
        for d in t.size: n *= d
        # contiguous only
        exp = []
        acc = 1
        for d in reversed(t.size):
            exp.insert(0, acc); acc *= d
        assert tuple(exp) == t.stride or n == 1, (k, t.size, t.stride)
        s = t.storage
        vals = struct.unpack_from('<%d%s' % (n, s.dtype), raw[s.key], (2 if s.dtype == 'e' else 4) * t.offset)
        out[k] = (t.size, vals)
    return out

def main(src, dst):
    w = load(src)
    keys = sorted(w, key=lambda k: (int(k.split('.')[1]), k.split('.')[2]))
    convs = [k for k in keys if w[k][0] and len(w[k][0]) == 4]
    print(len(keys), 'tensors,', len(convs), 'convolutions; first', w[convs[0]][0], 'last', w[convs[-1]][0])
    with open(dst, 'wb') as f:
        f.write(b'SRVG')
        f.write(struct.pack('<5I', 3, 64, len(convs) - 2, 4, 1))  # in ch, features, middle convs, scale, prelu
        for k in keys:
            size, vals = w[k]
            f.write(struct.pack('<%df' % len(vals), *vals))
    print('wrote', dst)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
