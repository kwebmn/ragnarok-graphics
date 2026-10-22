#!/usr/bin/env python3
"""Publishes a release on uaro.kiev.ua (2.10): its archives into the site's files/graphics/ and update.ini naming them.

The settings app reads https://uaro.kiev.ua/files/graphics/update.ini (src/core/update_check, format 1) and downloads
the archives it names from beside it; nginx serves them as they are (a broken download continues with Range).

  publish_site.py WEB_DIR VERSION ZIP SETUP [--dlss ZIP VERSION] [--models ZIP VERSION] [--withdraw V ...]
  publish_site.py --self-test

DLSS and the models change seldom: without --dlss or --models the sections of the update.ini there stay. A withdrawn
version (--withdraw) is offered the published one even when that is older - the way back from a bad release, every
release going to everyone. The newest 5 package archives stay.
"""
import argparse
import hashlib
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

KEEP = 5


def safe(version):
    return re.sub(r"[^A-Za-z0-9.]", "-", version)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for part in iter(lambda: f.read(1 << 20), b""):
            h.update(part)
    return h.hexdigest()


def put(source, web, name):
    """The file into the site's folder under `name`, through a temporary file and a rename."""
    target = Path(web) / name
    if target.is_file() and sha256(target) == sha256(source):
        return target
    tmp = Path(web) / (name + ".part")
    shutil.copyfile(source, tmp)
    os.replace(tmp, target)
    return target


def sections(text):
    """The sections of an update.ini: name -> its lines."""
    out, name = {}, None
    for line in text.splitlines():
        m = re.fullmatch(r"\s*\[([^\]]+)\]\s*", line)
        if m:
            name = m.group(1).strip().lower()
            out[name] = []
        elif name is not None and line.strip():
            out[name].append(line.strip())
    return out


def value(lines, key):
    for line in lines:
        k, _, v = line.partition("=")
        if k.strip().lower() == key:
            return v.strip()
    return ""


def archive(name, web, file, version=None):
    path = Path(web) / file
    lines = ["[%s]" % name]
    if version:
        lines.append("version = " + version)
    lines += ["file = " + file, "size = %d" % path.stat().st_size, "sha256 = " + sha256(path)]
    return lines


def publish(web, version, zip_path, setup_path, dlss=None, models=None, withdraw=(), keep=KEEP):
    web = Path(web)
    web.mkdir(parents=True, exist_ok=True)
    old = sections((web / "update.ini").read_text("ascii")) if (web / "update.ini").is_file() else {}
    package = "uaro-graphics-%s.zip" % version
    put(zip_path, web, package)
    put(setup_path, web, "uaro-graphics-setup.exe")
    withdrawn = value(old.get("release", []), "withdrawn").split() + list(withdraw)
    withdrawn = [v for i, v in enumerate(withdrawn) if v != version and v not in withdrawn[:i]]
    lines = ["; uaRO Graphics: the newest release, for the settings app (tools/publish_site.py)", "[release]",
             "format = 1", "version = " + version]
    if withdrawn:
        lines.append("withdrawn = " + " ".join(withdrawn))
    lines += archive("package", web, package)
    for name, given in (("dlss", dlss), ("models", models)):
        if given:
            file = "uaro-graphics-%s-%s.zip" % (name, safe(given[1]))
            put(given[0], web, file)
            lines += archive(name, web, file, given[1])
        elif name in old:
            lines += ["[%s]" % name] + old[name]
    text = "\r\n".join(lines) + "\r\n"
    fd, tmp = tempfile.mkstemp(dir=str(web), prefix="update.ini.")
    with os.fdopen(fd, "wb") as f:
        f.write(text.encode("ascii"))
    os.chmod(tmp, 0o644)
    os.replace(tmp, web / "update.ini")
    packages = sorted((p for p in web.glob("uaro-graphics-[0-9]*.zip") if p.name != package),
                      key=lambda p: p.stat().st_mtime, reverse=True)
    for p in packages[keep - 1:]:
        p.unlink()
    return text


def self_test():
    with tempfile.TemporaryDirectory() as d:
        src = Path(d) / "src"
        src.mkdir()
        (src / "z.zip").write_bytes(b"PK")
        (src / "s.exe").write_bytes(b"MZ")
        text = publish(Path(d) / "web", "2.10.0", src / "z.zip", src / "s.exe")
        s = sections(text)
        ok = value(s.get("release", []), "format") == "1" and value(s.get("package", []), "size") == "2" and \
            re.fullmatch(r"[0-9a-f]{64}", value(s.get("package", []), "sha256")) is not None
        print("OK: publish_site.py writes update.ini format 1" if ok else "FAIL: publish_site.py's update.ini")
        return 0 if ok else 1


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("web")
    p.add_argument("version")
    p.add_argument("zip")
    p.add_argument("setup")
    p.add_argument("--dlss", nargs=2, metavar=("ZIP", "VERSION"))
    p.add_argument("--models", nargs=2, metavar=("ZIP", "VERSION"))
    p.add_argument("--withdraw", nargs="+", default=[])
    a = p.parse_args(argv)
    if not re.fullmatch(r"\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?", a.version):
        print("FAIL: %s is not a version" % a.version)
        return 1
    sys.stdout.write(publish(a.web, a.version, a.zip, a.setup, a.dlss, a.models, a.withdraw).replace("\r\n", "\n"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
