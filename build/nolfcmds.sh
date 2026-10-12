#!/usr/bin/env bash
# The game directory's nolfcmds.txt, as a fresh build writes it:
#
#   build/nolfcmds.sh <retail|open>
#
# The retail NOLF.exe launcher writes nolfcmds.txt and runs
# "lithtech.exe -cmdfile nolfcmds.txt"; run.sh does the same with this one.
# Rez order is the GOTY install's (later entries override earlier ones), and
# custom/ -- our game DLLs -- goes last. +DisableMovies skips the Bink intro
# logos, which retail binkw32.dll misrenders or stalls on under some Wine
# setups; the modern build's own Bink player (open middleware) plays them.
# build/all.sh writes it into the game directory, build/package.sh into the
# archive (the launcher rewrites the directory's copy when it is used).
MOVIES=" +DisableMovies 1"
[ "${1:-retail}" = open ] && MOVIES=""
printf -- '-rez nolf.rez -rez nolf2.rez -rez nolfu003.rez -rez nolfgoty.rez -rez custom ++screenwidth 640 ++screenheight 480 ++bitdepth 16 ++windowed 1%s\r\n' "$MOVIES"
