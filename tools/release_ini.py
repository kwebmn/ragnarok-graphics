#!/usr/bin/env python3
"""Writes a branch's release/update.ini (3.0.0-beta.2): its newest release and the archives, each by the commit that
holds it - the settings app reads it from raw.githubusercontent.com (src/core/update_check, format 2).

  release_ini.py RELEASE_DIR VERSION REF [--dlss VERSION] [--models VERSION] [--withdraw V ...]
  release_ini.py --self-test

RELEASE_DIR is the repository's release/ folder as commit REF holds it (the archives' commit): uaro-graphics.zip, and
uaro-graphics-dlss.zip and uaro-graphics-models.zip with the versions --dlss and --models name. A withdrawn version
(--withdraw, and the old file's withdrawn line) is offered the release even when that is older - the way back from a
bad release.
"""
import argparse
import hashlib
import os
import re
import sys
import tempfile
from pathlib import Path

FILES = {"package": "uaro-graphics.zip", "dlss": "uaro-graphics-dlss.zip", "models": "uaro-graphics-models.zip"}
VERSION = r"\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for part in iter(lambda: f.read(1 << 20), b""):
            h.update(part)
    return h.hexdigest()


def sections(text):
    """The sections of an update.ini: name -> its lines."""
    out, name = {}, None
    for line in text.splitlines():
        m = re.fullmatch(r"\s*\[([^\]]+)\]\s*", line)
        if m:
            name = m.group(1).strip().lower()
            out[name] = []
        elif name is not None and line.strip() and not line.strip().startswith(";"):
            out[name].append(line.strip())
    return out


def value(lines, key):
    for line in lines:
        k, _, v = line.partition("=")
        if k.strip().lower() == key:
            return v.strip()
    return ""


def write(release, version, ref, dlss=None, models=None, withdraw=()):
    """release/update.ini written (through a temporary file and a rename); its text."""
    if not re.fullmatch(VERSION, version):
        raise ValueError("%s is not a version" % version)
    if not re.fullmatch(r"[0-9a-f]{40}", ref):
        raise ValueError("%s is not a commit (40 lower-case hex digits)" % ref)
    release = Path(release)
    ini = release / "update.ini"
    old = sections(ini.read_text("ascii")) if ini.is_file() else {}
    withdrawn = value(old.get("release", []), "withdrawn").split() + list(withdraw)
    withdrawn = [v for i, v in enumerate(withdrawn) if v != version and v not in withdrawn[:i]]
    lines = ["; uaRO Graphics: the newest release of this branch, for the settings app (tools/release_ini.py)",
             "[release]", "format = 2", "version = " + version]
    if withdrawn:
        lines.append("withdrawn = " + " ".join(withdrawn))
    for name, given in (("package", version), ("dlss", dlss), ("models", models)):
        if given is None:
            continue
        path = release / FILES[name]
        if not path.is_file():
            raise ValueError("no %s" % path)
        lines.append("[%s]" % name)
        if name != "package":
            lines.append("version = " + given)
        lines += ["ref = " + ref, "file = release/" + FILES[name], "size = %d" % path.stat().st_size,
                  "sha256 = " + sha256(path)]
    text = "\r\n".join(lines) + "\r\n"
    fd, tmp = tempfile.mkstemp(dir=str(release), prefix="update.ini.")
    with os.fdopen(fd, "wb") as f:
        f.write(text.encode("ascii"))
    os.chmod(tmp, 0o644)
    os.replace(tmp, ini)
    return text


def self_test():
    ref = "0dfe661d7f5f3eb871c5defcd398a543196dbfd8"
    with tempfile.TemporaryDirectory() as d:
        release = Path(d)
        (release / FILES["package"]).write_bytes(b"PK package")
        (release / FILES["models"]).write_bytes(b"PK models")
        (release / "update.ini").write_text("[release]\r\nformat = 2\r\nversion = 3.0.0-beta.1\r\n"
                                            "withdrawn = 2.9.1\r\n", "ascii")
        s = sections(write(release, "3.0.0-beta.2", ref, models="models-2 1", withdraw=["3.0.0-beta.1"]))
        top, package, models = s.get("release", []), s.get("package", []), s.get("models", [])
        ok = value(top, "format") == "2" and value(top, "version") == "3.0.0-beta.2" \
            and value(top, "withdrawn") == "2.9.1 3.0.0-beta.1" \
            and value(package, "ref") == ref and value(package, "file") == "release/uaro-graphics.zip" \
            and value(package, "size") == "10" \
            and value(package, "sha256") == hashlib.sha256(b"PK package").hexdigest() \
            and value(models, "version") == "models-2 1" \
            and value(models, "file") == "release/uaro-graphics-models.zip" and "dlss" not in s
        for bad in (("3.0", ref), ("3.0.0", ref[:39]), ("3.0.0", ref.upper())):
            try:
                write(release, bad[0], bad[1])
                ok = False
            except ValueError:
                pass
        try:
            write(release, "3.0.0", ref, dlss="streamline 2.14.1")  # no DLSS zip there
            ok = False
        except ValueError:
            pass
    print("OK: release_ini.py writes update.ini format 2" if ok else "FAIL: release_ini.py's update.ini")
    return 0 if ok else 1


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("release")
    p.add_argument("version")
    p.add_argument("ref")
    p.add_argument("--dlss")
    p.add_argument("--models")
    p.add_argument("--withdraw", nargs="+", default=[])
    a = p.parse_args(argv)
    try:
        sys.stdout.write(write(a.release, a.version, a.ref, a.dlss, a.models, a.withdraw))
    except ValueError as e:
        print("FAIL: %s" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
