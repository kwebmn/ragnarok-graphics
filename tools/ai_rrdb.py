"""Converts Real-ESRGAN's RRDBNet checkpoint RealESRGAN_x4plus_anime_6B.pth to the "RRDB" file src/core/ai/rrdb.cpp
reads (1.5). The same restricted unpickler as tools/ai_model.py: no code from the file runs.
Usage: python3 -I tools/ai_rrdb.py RealESRGAN_x4plus_anime_6B.pth dist/ai/realesrgan-x4plus-anime-6b.bin
The model: github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth (BSD-3-Clause,
dist/ai/LICENSE-Real-ESRGAN.txt).
2.5.27: the old ESRGAN's names too (model.0, model.1.sub.N.RDBr.convk.0, model.3/6/8/10), and --half writes the
weights as halves (format 2, half the size; 8-bit pictures within 2 levels): Game AI 2.0 (by Tal, WTFPL, openmodeldb
4x-GameAI-2-0) - python3 -I tools/ai_rrdb.py --half 4x_GameAI_2.0.pth 4x-gameai-2.0.bin, then
tools/models_package.py."""
import os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai_model import load  # noqa: E402


def esrgan_names(w):
    """The old ESRGAN's keys as the new RRDBNet's."""
    out = {}
    for k, v in w.items():
        n = k
        m = re.match(r'model\.1\.sub\.(\d+)\.RDB(\d)\.conv(\d)\.0\.(weight|bias)$', k)
        if m:
            n = 'body.%s.rdb%s.conv%s.%s' % m.groups()
        elif re.match(r'model\.1\.sub\.\d+\.(weight|bias)$', k):
            n = 'conv_body.' + k.rsplit('.', 1)[1]
        else:
            for a, b in (('model.0.', 'conv_first.'), ('model.3.', 'conv_up1.'), ('model.6.', 'conv_up2.'),
                         ('model.8.', 'conv_hr.'), ('model.10.', 'conv_last.')):
                if k.startswith(a):
                    n = b + k[len(a):]
        out[n] = v
    return out


def main(src, dst, half=False):
    w = esrgan_names(load(src))
    f = w['conv_first.weight'][0][0]
    g = w['body.0.rdb1.conv1.weight'][0][0]
    blocks = 1 + max(int(k.split('.')[1]) for k in w if k.startswith('body.'))
    order = ['conv_first']
    for b in range(blocks):
        for r in range(1, 4):
            for k in range(1, 6):
                order.append('body.%d.rdb%d.conv%d' % (b, r, k))
    order += ['conv_body', 'conv_up1', 'conv_up2', 'conv_hr', 'conv_last']
    assert len(order) * 2 == len(w), (len(order), len(w))
    with open(dst, 'wb') as out:
        out.write(b'RRDB')
        out.write(struct.pack('<5I', f, g, blocks, 4, 2 if half else 1))
        for name in order:
            for part in ('bias', 'weight'):
                vals = w[name + '.' + part][1]
                out.write(struct.pack('<%d%s' % (len(vals), 'e' if half else 'f'), *vals))
    print('F %d, G %d, %d blocks%s: wrote %s' % (f, g, blocks, ', halves' if half else '', dst))


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--half']
    main(args[0], args[1], '--half' in sys.argv[1:])
