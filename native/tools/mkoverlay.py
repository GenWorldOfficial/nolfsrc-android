#!/usr/bin/env python3
"""Make the clang VFS overlay that lets the Windows-written source compile
unchanged on a case-sensitive file system.

The source was written on Windows: it includes "StdAfx.h" for stdafx.h,
<Windows.h> for windows.h, and "..\\Shared\\NetDefs.h" with backslashes. The
Windows build must stay byte-identical (an include's spelling can end up in
__FILE__), so rather than edit the includes, the native build compiles with
`-ivfsoverlay <this file's output>`:

  * every file under the given roots is listed in a case-insensitive overlay,
    so any spelling of its name finds it;
  * every #include "..." written with backslashes gets an entry of its own,
    named with the backslashes, under each directory where the path it
    names exists (the including file's directory, and include-path
    directories alike), pointing at the real file.

    mkoverlay.py <output.yaml> <root> [<root>...]
"""

import json
import os
import re
import sys

INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]*\\[^"]*)"', re.M)
SOURCE_EXT = {".h", ".hpp", ".hh", ".inl", ".c", ".cpp", ".cc", ".cxx", ".rc", ".y", ".l"}


def find_real(directory, relpath):
    """The real path of directory/relpath, matching each component without
    case, or None."""
    path = directory
    for part in relpath.replace("\\", "/").split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            path = os.path.dirname(path)
            continue
        try:
            names = os.listdir(path)
        except OSError:
            return None
        match = [n for n in names if n.lower() == part.lower()]
        if not match:
            return None
        path = os.path.join(path, match[0])
    return path if os.path.isfile(path) else None


def main():
    out, roots = sys.argv[1], [os.path.abspath(r) for r in sys.argv[2:]]
    dirs = {}            # absolute dir -> {name: real path}
    includes = set()     # backslashed include strings
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in (".git", "Release", "Debug", "Final_Release")]
            entries = dirs.setdefault(dirpath, {})
            for name in filenames:
                full = os.path.join(dirpath, name)
                entries[name] = full
                if os.path.splitext(name)[1].lower() in SOURCE_EXT:
                    try:
                        with open(full, encoding="latin-1") as f:
                            includes.update(INCLUDE.findall(f.read()))
                    except OSError:
                        pass

    for inc in sorted(includes):
        for d in list(dirs):
            real = find_real(d, inc)
            if real:
                dirs[d][inc] = real

    overlay = {
        "version": 0,
        "case-sensitive": "false",
        "roots": [
            {
                "type": "directory",
                "name": d,
                "contents": [
                    {"type": "file", "name": n, "external-contents": p}
                    for n, p in sorted(entries.items())
                ],
            }
            for d, entries in sorted(dirs.items())
            if entries
        ],
    }
    tmp = out + ".tmp"
    with open(tmp, "w") as f:
        json.dump(overlay, f, indent=0)
    os.replace(tmp, out)
    print(f"mkoverlay: {sum(len(e) for e in dirs.values())} entries in {len(dirs)} directories, "
          f"{len(includes)} backslashed includes")


if __name__ == "__main__":
    main()
