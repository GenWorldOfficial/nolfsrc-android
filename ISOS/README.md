# ISOS/ -- the disc images you provide

The build takes every byte of game data -- and the licensed middleware
runtime (Miles Sound System, Bink) -- from the original discs, so it needs
images of the two CDs of **No One Lives Forever: Game of the Year Edition**
(PC, 2001):

| File | Disc | Holds |
|---|---|---|
| `NOLF1-CD1.iso` | CD 1 (install disc) | the InstallShield data; `Data/NOLF2.REZ`, `Data/nolfu003.rez`, `Data/NOLFGOTY.REZ` |
| `NOLF1-CD2.iso` | CD 2 (play disc)    | `Game/` (the installed game's files), `Data/NOLF.REZ`, the movies |

Put them here under exactly those names (copies or symlinks; they are
gitignored), or point `NOLF_ISOS=<dir>` at a directory holding them.

They must be plain ISO 9660 images of the discs (e.g. `dd if=/dev/cdrom
of=NOLF1-CD1.iso bs=2048`), and `make` refuses to build unless both match
`SHA256SUMS`:

```sh
cd ISOS && sha256sum -c SHA256SUMS
```

An image that does not match -- a different edition, a different language, a
damaged rip -- is rejected before anything is built.

Nothing from the discs is part of this repository: the build reads them
into `out/nolf-legacy-release/` (and the launcher's art into `src/engine/launcher/res/`,
which is gitignored) every time. Which files are taken, and how, is
`tools/discextract.py`.
