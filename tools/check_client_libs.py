#!/usr/bin/env python3
"""check_client_libs.py BUILD_DIR: our own libraries in place of the client's old ones (3.0.1-beta.5).

Each built DLL is 32-bit, exports exactly what uaRO.exe takes of it (the names; the ordinals where the exe takes them
by ordinal), imports Windows' own DLLs only, and carries the marker the installer tells ours from the game's by.
"""
import struct
import sys

MARK = b"uaRO Graphics client library"
ALLOWED_IMPORTS = {"kernel32.dll", "msvcrt.dll", "user32.dll", "ole32.dll", "winmm.dll"}
# uaRO.exe's import table (sha256 63902c46...): name -> the ordinal it must have, None when taken by name
LIBS = {
    "cps.dll": {"compress": 1, "uncompress": 2},
    "ijl15.dll": {"ijlGetLibVersion": 1, "ijlInit": 2, "ijlFree": 3, "ijlRead": 4, "ijlWrite": 5, "ijlErrorStr": 6},
    "binkw32.dll": {n: None for n in [
        "_BinkOpen@8", "_BinkClose@4", "_BinkDoFrame@4", "_BinkNextFrame@4", "_BinkCopyToBuffer@28",
        "_BinkDDSurfaceType@4", "_BinkWait@4", "_BinkPause@8", "_BinkGoto@12", "_BinkOpenDirectSound@4",
        "_BinkSetSoundSystem@8"]},
    # 3.0.1-beta.7: Miles Sound System 6's 46, and _AIL_set_sample_pan@8 the driver looks up (win/sound_hook.cpp)
    "mss32.dll": {n: None for n in [
        "_AIL_init_sample@4", "_AIL_set_sample_file@12", "_AIL_set_sample_volume@8", "_AIL_start_sample@4",
        "_AIL_end_sample@4", "_AIL_end_3D_sample@4", "_AIL_open_stream@12", "_AIL_set_stream_loop_count@8",
        "_AIL_start_stream@4", "_AIL_pause_stream@8", "_AIL_stream_volume@4", "_AIL_set_stream_volume@8",
        "_AIL_release_3D_sample_handle@4", "_AIL_close_3D_listener@4", "_AIL_sample_status@4",
        "_AIL_release_sample_handle@4", "_AIL_close_digital_driver@4", "_AIL_shutdown@0",
        "_AIL_set_redist_directory@4", "_AIL_startup@0", "_AIL_set_preference@8", "_AIL_open_digital_driver@16",
        "_AIL_allocate_sample_handle@4", "_AIL_enumerate_3D_providers@12", "_AIL_open_3D_provider@4",
        "_AIL_3D_speaker_type@4", "_AIL_set_3D_speaker_type@8", "_AIL_open_3D_listener@4",
        "_AIL_allocate_3D_sample_handle@4", "_AIL_start_3D_sample@4", "_AIL_set_3D_sample_distances@12",
        "_AIL_set_3D_sample_volume@8", "_AIL_close_3D_provider@4", "_AIL_set_3D_orientation@28",
        "_AIL_set_3D_position@16", "_AIL_set_3D_sample_effects_level@8", "_AIL_set_3D_sample_file@8",
        "_AIL_3D_sample_status@4", "_AIL_set_3D_room_type@8", "_AIL_mem_free_lock@4", "_AIL_decompress_ADPCM@12",
        "_AIL_WAV_info@8", "_AIL_decompress_ASI@24", "_AIL_file_type@8", "_AIL_3D_room_type@4",
        "_AIL_close_stream@4",
        "_AIL_set_sample_pan@8"]},
}


def read(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    machine, nsec = struct.unpack_from("<HH", data, pe + 4)
    if machine != 0x14C:
        raise ValueError("not 32-bit (i386)")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    exp_rva = struct.unpack_from("<I", data, opt + 96)[0]
    imp_rva = struct.unpack_from("<I", data, opt + 104)[0]
    sections = [struct.unpack_from("<8sIIII", data, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vsize, va, rsize, raw in sections:
            if va <= rva < va + max(vsize, rsize):
                return rva - va + raw
        raise ValueError("rva %#x is outside the sections" % rva)

    def cstr(rva):
        o = off(rva)
        return data[o:data.index(b"\0", o)].decode()

    base, _, nnames, _, names, ords = struct.unpack_from("<IIIIII", data, off(exp_rva) + 16)
    exports = {}
    for i in range(nnames):
        exports[cstr(struct.unpack_from("<I", data, off(names) + 4 * i)[0])] = \
            base + struct.unpack_from("<H", data, off(ords) + 2 * i)[0]
    imports = []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        imports.append(cstr(struct.unpack_from("<I", data, o + 12)[0]).lower())
        o += 20
    return data, exports, imports


def check(path, want):
    data, exports, imports = read(path)
    if set(exports) != set(want):
        return "exports %s, not %s" % (sorted(exports), sorted(want))
    for name, ordinal in want.items():
        if ordinal is not None and exports[name] != ordinal:
            return "%s is ordinal %d, not %d" % (name, exports[name], ordinal)
    bad = [d for d in imports if d not in ALLOWED_IMPORTS]
    if bad:
        return "unexpected imports: " + ", ".join(bad)
    if MARK not in data:
        return "no marker (%s)" % MARK.decode()
    return ""


def main(build):
    failed = False
    for name, want in LIBS.items():
        try:
            why = check(build + "/" + name, want)
        except (OSError, ValueError, struct.error) as e:
            why = str(e)
        if why:
            print("FAIL: %s: %s" % (name, why))
            failed = True
        else:
            print("OK: %s: %d exports as uaRO.exe takes them, the marker" % (name, len(want)))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win")
