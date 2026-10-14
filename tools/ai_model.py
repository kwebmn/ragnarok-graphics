"""Converts a Real-ESRGAN SRVGGNetCompact checkpoint (.pth) to the "SRVG" file src/core/ai/srvgg.cpp reads.

Without torch: a restricted unpickler that only builds tensor descriptions (no code from the file runs), then the
weights as little-endian float32 in our order. Usage: python3 -I tools/ai_model.py realesr-general-x4v3.pth out.bin
The models in dist/ai/ came from github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/: realesr-general-x4v3.pth,
realesr-general-wdn-x4v3.pth and realesr-animevideov3.pth (16 convolutions instead of 32; BSD-3-Clause,
dist/ai/LICENSE-Real-ESRGAN.txt)."""
import pickle, struct, sys, zipfile

class Storage:
    def __init__(self, key, numel):
        self.key, self.numel = key, numel

class Tensor:
    def __init__(self, storage, offset, size, stride):
        self.storage, self.offset, self.size, self.stride = storage, offset, tuple(size), tuple(stride)

def rebuild_tensor_v2(storage, offset, size, stride, *rest):
    return Tensor(storage, offset, size, stride)

class OrderedDict(dict):
    pass

ALLOWED = {
    ('collections', 'OrderedDict'): OrderedDict,
    ('torch._utils', '_rebuild_tensor_v2'): rebuild_tensor_v2,
    ('torch', 'FloatStorage'): 'float',
}

class Safe(pickle.Unpickler):
    def find_class(self, module, name):
        if (module, name) in ALLOWED:
            return ALLOWED[(module, name)]
        raise pickle.UnpicklingError(f'refused {module}.{name}')
    def persistent_load(self, pid):
        kind, storage_type, key, location, numel = pid
        if kind != 'storage' or storage_type != 'float':
            raise pickle.UnpicklingError(f'refused storage {pid!r}')
        return Storage(key, numel)

def load(path):
    z = zipfile.ZipFile(path)
    names = z.namelist()
    pkl = [n for n in names if n.endswith('/data.pkl')][0]
    root = pkl[:-len('data.pkl')]
    obj = Safe(z.open(pkl)).load()
    if 'params_ema' in obj: obj = obj['params_ema']
    elif 'params' in obj: obj = obj['params']
    out = {}
    for k, t in obj.items():
        raw = z.read(root + 'data/' + t.storage.key)
        n = 1
        for d in t.size: n *= d
        # contiguous only
        exp = []
        acc = 1
        for d in reversed(t.size):
            exp.insert(0, acc); acc *= d
        assert tuple(exp) == t.stride or n == 1, (k, t.size, t.stride)
        vals = struct.unpack_from('<%df' % n, raw, 4 * t.offset)
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
