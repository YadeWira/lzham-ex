#!/usr/bin/env python3
# Checks tools/lzham_ref.py against streams produced by tools/lzham_compress.c over a few fixtures.
#
# Usage: test_reference.py <path to lzham_compress> [work directory]
#
# LZHAM is in the Public Domain. See LICENSE.
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lzham_ref


def fixtures():
    # Compressible text (matches, rep matches, delta literals).
    text = (b"LZHAM reference decoder test.\n" * 700) + bytes(range(256)) * 8
    # Incompressible bytes (raw blocks).
    seed = 0x12345678
    random_bytes = bytearray()
    for _ in range(65536):
        seed = (seed * 1103515245 + 12345) & 0xFFFFFFFF
        random_bytes.append((seed >> 16) & 0xFF)
    # Highly repetitive (huge matches) and a tiny input.
    return [
        ("empty", b""),
        ("tiny", b"abcd"),
        ("text", text),
        ("random", bytes(random_bytes)),
        ("runs", b"\x00" * 100000 + b"\xff" * 100000),
    ]


def main(argv):
    if len(argv) < 2:
        print("usage: %s <lzham_compress binary> [workdir]" % argv[0])
        return 2
    compressor = argv[1]
    workdir = argv[2] if len(argv) > 2 else tempfile.mkdtemp(prefix="lzham_ref_")
    os.makedirs(workdir, exist_ok=True)

    failures = 0
    cases = 0
    for name, data in fixtures():
        src = os.path.join(workdir, name + ".bin")
        with open(src, "wb") as f:
            f.write(data)
        for dict_size_log2 in (15, 20):
            for level in (0, 2, 4):
                cases += 1
                cmp_path = os.path.join(workdir, "%s_%d_%d.lz" % (name, dict_size_log2, level))
                result = subprocess.run([compressor, src, cmp_path, str(dict_size_log2), str(level)],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if result.returncode != 0:
                    print("FAIL %s dict=%d level=%d: compressor returned %d: %s" %
                          (name, dict_size_log2, level, result.returncode, result.stdout.decode(errors="replace")))
                    failures += 1
                    continue
                with open(cmp_path, "rb") as f:
                    stream = f.read()
                try:
                    out = lzham_ref.decompress(stream, dict_size_log2)
                except Exception as exc:  # noqa: BLE001 - report any decoder failure
                    print("FAIL %s dict=%d level=%d: %s" % (name, dict_size_log2, level, exc))
                    failures += 1
                    continue
                if out != data:
                    print("FAIL %s dict=%d level=%d: decoded %d bytes, expected %d" %
                          (name, dict_size_log2, level, len(out), len(data)))
                    failures += 1

    if failures:
        print("%d of %d case(s) FAILED" % (failures, cases))
        return 1
    print("Reference decoder OK (%d cases)." % cases)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
