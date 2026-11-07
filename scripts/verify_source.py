#!/usr/bin/env python3
"""Check the exact source snapshot before building; requires Python 3.9+."""
import hashlib
from pathlib import Path, PurePosixPath


def verify(root):
    entries = (root / 'SOURCE-MANIFEST.sha256').read_text().splitlines()
    seen = set()
    for line in entries:
        expected, name = line.split('  ', 1)
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or name in seen:
            raise SystemExit('Invalid or repeated source manifest path')
        seen.add(name)
        source = root / name
        if source.is_symlink() or not source.is_file():
            raise SystemExit('Missing or linked source input: ' + name)
        if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise SystemExit('Source checksum mismatch: ' + name)
    print('Verified {} source files'.format(len(seen)))


if __name__ == '__main__':
    verify(Path(__file__).resolve().parents[1])
