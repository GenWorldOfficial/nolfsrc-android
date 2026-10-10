# src/modern/ -- the modern build's own code and data

Everything here is used only by the modern build (`make modern`,
`NOLFSRC_MODERN`; `BUILDING.md`, "Variant builds"), never by the release
build, which stays the retail game. `MODERN.md` is the plan and the record of
what has been done.

| Directory | What | Used by |
|---|---|---|
| `cshell/` | new game code for `CShell.dll`: every `.cpp` here is compiled into it (with `CShell.dll`'s own flags and include paths, without its precompiled header -- include `stdafx.h` yourself) | `build/build.sh cshell` |
| `object/` | the same for `Object.lto` | `build/build.sh object` |
| `cres/` | new strings for `CRes.dll`: every `.rc` here is compiled into it beside `ClientRes.rc`; put the IDs in a `.h` beside it | `build/build.sh cres` |
| `data/` | game-data recipes, applied to the data from the discs (`data/README.md`) | `build/variant.sh`, `tools/moddata.py` |

In a modern build `src/modern/` is on the game DLLs' include path, so retail
files reach the new code with, for example, `#include "cshell/p1_input.h"`
inside an `#ifdef NOLFSRC_MODERN` block. Neither `CShell.dll` nor
`Object.lto` needs a project-file (`.dsp`) change for a new file, and no new
string goes into `ClientRes.rc`.

Conventions, so that work done in parallel merges cleanly:

* One file per package and topic, named for it: `cshell/p1_input.cpp`,
  `cshell/p2_console.cpp`, `cres/p3.rc` + `cres/p3.h`.
* String IDs from the package's range (`MODERN.md`); wording of our own.
* In retail files, each change is an `#ifdef NOLFSRC_MODERN` block opened by
  a comment naming the item (`// M-P1-01: frame limiter`), preferably a single
  call into the package's file, with the retail code kept in the `#else`.
