"""Converts Real-ESRGAN's RRDBNet checkpoint RealESRGAN_x4plus_anime_6B.pth to the "RRDB" file src/core/ai/rrdb.cpp
reads (1.5). The same restricted unpickler as tools/ai_model.py: no code from the file runs.
Usage: python3 -I tools/ai_rrdb.py RealESRGAN_x4plus_anime_6B.pth dist/ai/realesrgan-x4plus-anime-6b.bin
The model: github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth (BSD-3-Clause,
dist/ai/LICENSE-Real-ESRGAN.txt)."""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai_model import load  # noqa: E402


def main(src, dst):
    w = load(src)
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
        out.write(struct.pack('<5I', f, g, blocks, 4, 1))
        for name in order:
            for part in ('bias', 'weight'):
                vals = w[name + '.' + part][1]
                out.write(struct.pack('<%df' % len(vals), *vals))
    print('F %d, G %d, %d blocks: wrote %s' % (f, g, blocks, dst))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
