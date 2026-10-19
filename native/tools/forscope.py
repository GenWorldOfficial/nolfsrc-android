#!/usr/bin/env python3
"""Fix Visual C++ 6.0's for-loop scoping, which clang does not have.

VC6 scopes a variable declared in a for statement to the block around the
loop, so code uses it after the loop:

    for (int i = 0; i < n; i++) ...
    if (i == n) ...                     // fine in VC6, an error in C++98

For each "use of undeclared identifier" in a clang build log, this finds the
nearest for statement above it, in the same function, that declares that
name, and moves the declaration in front of the loop on the same line:

    int i; for (i = 0; i < n; i++) ...

That is what VC6 compiled already (the same variable, in the same block),
and the line numbers stay as they were: asserts put __LINE__ into the
Windows binaries. A loop whose position makes that unsafe (the body of an
unbraced if, else, while or for) is listed for a person to fix instead.

    forscope.py <build log> [--dry-run]
"""

import re
import sys
from collections import defaultdict

ERROR = re.compile(r"^(/[^:]+):(\d+):\d+: error: use of undeclared identifier '([A-Za-z_]\w*)'")


def for_decl(var):
    # for ( [const] type [*...] var = ...   (one variable)
    return re.compile(r"\bfor\s*\(\s*((?:const\s+)?(?:unsigned\s+|signed\s+)?[A-Za-z_][\w:]*(?:\s*\*+)?)\s+(\*?\s*)"
                      + re.escape(var) + r"\s*=")


def main():
    log, dry = sys.argv[1], "--dry-run" in sys.argv
    uses = defaultdict(set)                 # file -> {(line, var)}
    for line in open(log, encoding="utf-8", errors="replace"):
        m = ERROR.match(line)
        if m:
            uses[m.group(1)].add((int(m.group(2)), m.group(3)))

    fixed = manual = 0
    for path, sites in sorted(uses.items()):
        with open(path, encoding="latin-1", newline="") as f:
            lines = f.read().split("\n")
        changed = False
        for lineno, var in sorted(sites):
            pat = for_decl(var)
            hit = None
            done = re.compile(r"\b" + re.escape(var) + r"; for \(" + re.escape(var) + r" =")
            for i in range(lineno - 1, max(-1, lineno - 400), -1):
                text = lines[i]
                if i < lineno - 1 and re.match(r"^\}", text):
                    break                   # the end of the previous function
                if done.search(text):
                    break                   # fixed for an earlier use
                m = pat.search(text)
                if m:
                    hit = (i, m)
                    break
            if not hit:
                continue
            i, m = hit
            text = lines[i]
            # the declaration must not be the whole body of an unbraced
            # if/else/while/for: what is before the for on its line, or the
            # line above, ends a statement or opens/closes a block
            before = text[:m.start()].strip()
            prev = ""
            for j in range(i - 1, -1, -1):
                p = lines[j].split("//")[0].strip()
                if p:
                    prev = p
                    break
            safe_before = before in ("", "{", "}") or before.endswith((";", "{", "}"))
            safe_prev = before != "" or prev.endswith((";", "{", "}", ":")) or prev.startswith("#")
            # one variable only: "for (int i = 0, j = 0;" is not handled
            rest = text[m.end():]
            init = rest.split(";", 1)[0]
            depth, comma = 0, False
            for ch in init:
                depth += ch in "(["
                depth -= ch in ")]"
                comma |= ch == "," and depth == 0
            if not (safe_before and safe_prev) or comma:
                print(f"MANUAL {path}:{i + 1}: {text.strip()}  (uses {var} at line {lineno})")
                manual += 1
                continue
            decl = m.group(1) + " " + m.group(2).replace(" ", "") + var + "; "
            new = text[:m.start()] + decl + "for (" + var + " =" + text[m.end():]
            if not dry:
                lines[i] = new
            changed = True
            fixed += 1
            print(f"fixed  {path}:{i + 1}: {new.strip()}")
        if changed and not dry:
            with open(path, "w", encoding="latin-1", newline="") as f:
                f.write("\n".join(lines))
    print(f"forscope: {fixed} loops fixed, {manual} to fix by hand")


if __name__ == "__main__":
    main()
