# progress/ -- how much of the build is byte-identical to the retail game

The goal of this source is to compile to exactly the machine code of the
retail game (*No One Lives Forever*, Game of the Year Edition, version
1.003). This directory holds the record of how far that has got, and the
evidence the scoring needs. The table in the top-level `README.md` is
generated from it.

```sh
make                                     # build (the scoring reads its output)
make progress NOLF_SOURCE=<dir>          # score it; fails on any regression
make progress-update NOLF_SOURCE=<dir>   # record the score here and in README.md
```

## What is scored

Every function of each retail binary is compared with what our build
produced for it:

| Target | Compared against | Our side |
|---|---|---|
| `lithtech.exe`, `server.dll`, `d3d.ren`, `NOLF.exe` | the retail images, extracted from disc 2 into `out/reference/` | `src/engine/*/Release/*.obj` (plus the lithshared libraries each links) |
| `StdLith`, `CryptMgr`, `RegMgr`, `MFCStub`, `ButeMgrNoMFC`, `ltguimgr_LT2`, `LithFontMgr_LT2` | Monolith's prebuilt `.lib`s from the NOLF Source Code v1.003 release (`NOLF_SOURCE`) | `src/lithshared/out/*.lib` |

The retail images have no symbols, so their functions are enumerated from the
code itself (call targets, vtables, callbacks, exports, ...), and each of our
compiled functions is searched for in the image. A function is
**identical** when our bytes equal the retail bytes everywhere except where
the linker writes an address (relocations, which an unlinked object leaves
blank). The libraries are compared symbol by symbol, the same way.

Each retail function gets one status (`out/progress/functions.csv`):

| Status | Meaning |
|---|---|
| `identical` | our code is the retail code, relocations aside |
| `near` | we have the function, but the bytes differ |
| `small` | we have it, but it is almost all relocations (global initializers), so there is nothing to compare: reconstructed, not verified |
| `attempted` | no match, but the address is cited in `src/` or recorded in the CSVs here |
| `todo` | nothing of ours corresponds to it yet |
| `crt` | the statically linked C runtime / MFC: listed, not counted |

`unplaced` counts functions of ours that occur nowhere in that image (most
are server code `lithtech.exe` compiles but never links); they are counted in
neither direction. The README's bars are identical bytes over all counted
bytes.

This is the per-function measure. A whole binary is byte-perfect only once
every function is identical *and* it links in the same order, with the same
data and headers; that final, whole-file comparison is not made yet.

## The test

`progress/identical.txt` lists every function verified identical, and
`progress/summary.json` the figures they add up to. `make progress`
(`build/progress.sh check`) re-scores the current build and fails if:

* a function listed in `identical.txt` is no longer identical,
* a target's identical byte count went down, or
* `README.md`'s table is not the one `summary.json` renders.

Newly identical functions pass, with a reminder to record them:
`make progress-update` rewrites `identical.txt`, `summary.json` and the
README table, so the improvement is locked in from then on. It refuses a run
that lost a recorded function unless `ACCEPT_LOSSES=1` is given (when a
change rightly gives one up -- say, a function was matched by coincidence),
and a run without the libraries, since the record must cover every target.

`make progress COMPLETE=1` also fails unless every function of every target
is identical: the finish line.

The scoring compiles nothing; it reads what the last `make` left in the
tree. Run `make` after changing the source, then `make progress`. It needs
Python 3 only, so inside `make shell` (or on a host with Python)
`build/progress.sh check` works directly.

## The libraries: `NOLF_SOURCE`

The seven lithshared libraries are scored against the prebuilt `.lib`s that
came with Monolith's *NOLF Source Code v1.003* release
(`LT2/lithshared/libs/Release/`). Like the disc images, they are not in this
repository: set `NOLF_SOURCE` to your extracted copy of the release (the
directory holding `LT2/`, or a directory holding the `.lib`s themselves).
They are checked against `shipped-libs.sha256`, and a mismatch is an error.

Without `NOLF_SOURCE`, the engine images are still scored and checked, and
the libraries are skipped with a note; `make progress-update` needs them.

## Files

| File | What |
|---|---|
| `identical.txt` | every function verified identical (`target<TAB>function`): the baseline the test holds the build to. Written by `make progress-update` |
| `summary.json` | per-target counts and bytes of the recorded run; the README table is rendered from it |
| `shipped-libs.sha256` | the source release's seven `.lib`s |
| `*-screen-exclude.csv` | per image: enumerated addresses examined by hand. A reason that starts `NOT A FUNCTION` or `NOT GAME CODE` removes the row from the count (an operand byte, a jump target inside another function, padding); any other reason only marks the address as `attempted` |
| `extent-splits.csv` | function starts the enumeration misses (a function hidden behind a thunk), with the evidence |
| `lithtech-decoded-map.csv` | `lithtech.exe` addresses a function of ours has been mapped to, marking them `attempted` |

The CSVs change what is counted, so a change to them is a change to the
denominator: give the evidence in the reason, as the existing rows do.

Per run, in `out/progress/`: `functions.csv` (every retail function and its
status) and `summary.json`. `tools/progress.py --nears` lists the near
matches closest first, and `--todo` / `--attempted` / `--small` the rows of
that status.
