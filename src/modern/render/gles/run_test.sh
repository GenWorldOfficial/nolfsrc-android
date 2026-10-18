#!/bin/sh
# Builds gles_test against gles_device.cpp and runs it under Xvfb with
# Mesa's software OpenGL ES 3 (llvmpipe). Needs: g++, libx11-dev,
# libgles-dev, libegl-dev, xvfb, libegl-mesa0.
#
#   src/modern/render/gles/run_test.sh [output dir]
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-${TMPDIR:-/tmp}/gles-test}
mkdir -p "$out"
g++ -std=c++17 -O1 -g -Wall -Wextra -o "$out/gles_test" \
    "$here/gles_test.cpp" "$here/gles_device.cpp" -lEGL -lGLESv2 -lX11
run() { EGL_PLATFORM=x11 LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" "$out/gles_test"; }
echo "==== with the driver's depth clamp and clip distance"
run
echo "==== without them (the shader's own clamping)"
NOLF_GLES_NOEXT=1 run
