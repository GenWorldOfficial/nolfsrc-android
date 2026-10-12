#!/usr/bin/env bash
# Compile and link: build/build.mk, in parallel, in the tree that is built.
#
#   build/compile.sh [target...]     (inside the build container)
#
# Targets are build/build.mk's: all (the default), libs, game, engine, a
# binary (cres cshell object ltmsg sres launcher server lithtech d3d), a
# binary's objects without its link (<binary>-objs), or any output file.
# Started in a checkout it builds in out/release-tree/ (src/ without its
# variant blocks: build/env.sh, nolfsrc_release_tree) and copies the outputs
# back into this checkout's src/; in a build/variant.sh tree, there.
# NOLFSRC_JOBS sets the number of parallel jobs (default: the CPU count).
set -e
source "$(dirname "$0")/env.sh"
nolfsrc_release_tree "$@"
cd "$REPO"
nolfsrc_make "${@:-all}"
