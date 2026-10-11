# src/modern/data/ -- the modern build's game-data edits

Some changes in the modern build (`make modern`, `NOLFSRC_MODERN`; see
`BUILDING.md`, "Variant builds") need game data changed as well as code. The
game data is not in this repository -- it comes from the discs -- so a change
is committed here as a recipe, and `tools/moddata.py` applies it when the
modern build is made. The results go into the game directory's `custom/` rez
directory, which the engine searches after the four `.rez` archives, so they
override the retail copies. The release build never sees any of it.

| Recipe | What it does |
|---|---|
| `<path>.patch` | a unified diff against the retail file at `<path>` (as the game loads it: the last `.rez` archive that holds it), e.g. `ATTRIBUTES/WEAPONS.txt.patch` |
| `files/<path>` | a new file, written for this project, installed at `<path>` |
| `<path>.from` | one line, `<archive> <path in it>`: that retail file, from that archive, installed at `<path>` -- for a retail file the Game of the Year archives override, wanted under another name (e.g. `INTERFACE/MAINMENU1B.dtx.from` containing `nolf2.rez INTERFACE/MAINMENU1.dtx`) |
| `<path>.bytes` | byte edits of a binary retail file, one per line: `<offset> <old hex> <new hex>` (offset in hex or decimal, negative to count from the end; `#` starts a comment); the old bytes must match, e.g. `INTERFACE/FONTS/HUD_CHOOSER.pcx.bytes` turns one palette entry black |
| `gamedir/...` | the same four kinds for a file the engine reads from the game directory itself rather than from the archives (`gamedir/defctrls.cfg.patch`); the retail original is kept beside it as `<file>.retail` and put back if the recipe goes |

To make a patch, edit a copy of the file and let the tool write the diff
(after a `make modern`, so `out/nolf-modern-release/` exists):

```sh
python3 tools/moddata.py show out/nolf-modern-release ATTRIBUTES/WEAPONS.txt > /tmp/weapons.txt
# ... edit /tmp/weapons.txt ...
python3 tools/moddata.py diff out/nolf-modern-release ATTRIBUTES/WEAPONS.txt /tmp/weapons.txt \
    > src/modern/data/ATTRIBUTES/WEAPONS.txt.patch
```

Keep patches small -- the changed lines and the three lines of context the
tool gives them. A patch has to apply exactly, so a retail file that does not
match what it expects fails the build rather than producing something half
edited. Never commit a whole retail file, and only put files here that were
written for this project; `MODERN.md` says which change each recipe belongs
to.
