#!/usr/bin/env python3
"""Builds build-dlss/uaro-graphics-dlss.zip: NVIDIA Streamline's and DLSS's DLLs for uaro-render64.exe (part 3),
taken out of the Streamline SDK release on GitHub by HTTP range requests (the SDK zip is 276 MB) and checked against
the hashes below. The package is unpacked into the uaRO client folder by get-dlss.bat: uaro-graphics/dlss/.
"""
import hashlib
import os
import struct
import sys
import urllib.request
import zipfile
import zlib
from pathlib import Path

VERSION = "streamline 2.14.1"  # also in dist/dlss-version.txt: get-dlss.bat downloads when it differs
SDK = "https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.1/streamline-sdk-v2.14.1.zip"
FILES = {
    "bin/x64/sl.interposer.dll": "8c87c9499461da561edd529aa9bf7831d67d7b94ebb1c1a5ed54ef4934e1ea4c",
    "bin/x64/sl.common.dll": "82924a8954dd671e09351c5de0eb87ad0eb25b944cc9f9ab955ca1d9950de15d",
    "bin/x64/sl.dlss.dll": "73bf52c0cfaa5900a8f3f4a91306e4625e7cca696dfb305aae44c9f97b582e1f",
    "bin/x64/sl.dlss_g.dll": "f4a6b2b14dcc0b1485989e430d3b4e3a44ac1800b92ba1ad74f476e64fb2b09c",
    "bin/x64/sl.reflex.dll": "0ce9725e3e03ea9e7f81d008b57f33ee365973d2e349131c8b1c3e3378fe2db0",
    "bin/x64/sl.pcl.dll": "f13d51cfa05f4cd514df2026049e2db8adf359221713170ad386fd499915b582",
    "bin/x64/nvngx_dlss.dll": "3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983",
    "bin/x64/nvngx_dlssg.dll": "ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82",
    "bin/x64/nvngx_dlss.license.txt": None,
    "license.txt": None,
}
OUT = Path(__file__).resolve().parent.parent / "build-dlss"


def get(start, end):
    req = urllib.request.Request(SDK, headers={"Range": "bytes=%d-%d" % (start, end)})
    with urllib.request.urlopen(req, timeout=300) as r:
        return r.read()


def members():
    """name -> (method, compressed size, local header offset) from the central directory."""
    head = urllib.request.urlopen(urllib.request.Request(SDK, headers={"Range": "bytes=0-0"}), timeout=120)
    size = int(head.headers["Content-Range"].split("/")[1])
    tail = get(size - 65536, size - 1)
    i = tail.rfind(b"PK\x05\x06")
    cd_size, cd_off = struct.unpack_from("<II", tail, i + 12)
    if cd_off == 0xFFFFFFFF or cd_size == 0xFFFFFFFF:
        j = tail.rfind(b"PK\x06\x06")
        cd_size, cd_off = struct.unpack_from("<QQ", tail, j + 40)
    cd = get(cd_off, cd_off + cd_size - 1)
    found = {}
    p = 0
    while p < len(cd) and cd[p:p + 4] == b"PK\x01\x02":
        method, = struct.unpack_from("<H", cd, p + 10)
        csize, usize = struct.unpack_from("<II", cd, p + 20)
        nlen, elen, clen = struct.unpack_from("<HHH", cd, p + 28)
        loff, = struct.unpack_from("<I", cd, p + 42)
        name = cd[p + 46:p + 46 + nlen].decode("cp437").replace("\\", "/")
        extra = cd[p + 46 + nlen:p + 46 + nlen + elen]
        if 0xFFFFFFFF in (csize, usize, loff):
            q = 0
            while q < len(extra):
                hid, hsz = struct.unpack_from("<HH", extra, q)
                if hid == 1:
                    vals = list(struct.unpack_from("<" + "Q" * (hsz // 8), extra, q + 4))
                    k = 0
                    if usize == 0xFFFFFFFF:
                        usize = vals[k]
                        k += 1
                    if csize == 0xFFFFFFFF:
                        csize = vals[k]
                        k += 1
                    if loff == 0xFFFFFFFF:
                        loff = vals[k]
                q += 4 + hsz
        found[name] = (method, csize, loff)
        p += 46 + nlen + elen + clen
    return found


def main():
    stage = OUT / "package" / "uaro-graphics" / "dlss"
    stage.mkdir(parents=True, exist_ok=True)
    have = members()
    for name, digest in FILES.items():
        if name not in have:
            sys.exit("FAIL: %s is not in the SDK" % name)
        method, csize, loff = have[name]
        lh = get(loff, loff + 29)
        ln, le = struct.unpack_from("<HH", lh, 26)
        data = get(loff + 30 + ln + le, loff + 30 + ln + le + csize - 1)
        if method == 8:
            data = zlib.decompress(data, -15)
        if digest and hashlib.sha256(data).hexdigest() != digest:
            sys.exit("FAIL: %s is not the file this build was made with" % name)
        target = "streamline.license.txt" if name == "license.txt" else Path(name).name
        (stage / target).write_bytes(data)
    (stage / "version.txt").write_bytes(VERSION.encode() + b"\r\n")
    out = OUT / "uaro-graphics-dlss.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(stage.iterdir()):
            z.write(f, "uaro-graphics/dlss/" + f.name)
    print("packed %s (%d MB): %s" % (out, out.stat().st_size >> 20, VERSION))


if __name__ == "__main__":
    main()
