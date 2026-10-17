# What the shared tools/ need to know about this project, for the scripts
# that call them (build/env.sh and build/variant.sh source this). The tools
# default to No One Lives Forever 2's layout; these name NOLF1's.

# tools/releasesrc.py: the variant macros the release build resolves away,
# the directories the build writes its outputs into, and the files it
# generates beside their sources (the engines' link inputs, flex's and
# bison's output, CRes.dll's Game of the Year script, the lithshared
# libraries).
export RELEASESRC_VARIANTS="NOLFSRC_MODERN NOLFSRC_DEBUG"
export RELEASESRC_OUTPUT_DIRS="Release Final_Release Debug"
export RELEASESRC_GENERATED='^(engine/[^/]+/link/.*\.(obj|lib|cmd)|game/ObjectDLL/(AnimationLex\.cpp|AnimationParse\.cpp|AnimationLexSymbols\.h)|game/ClientRes/goty/cres_goty\.rc|lithshared/out/.*)$'

# tools/mkdeps.py: the project's own headers, last on every INCLUDE path.
export MKDEPS_PROJECT_INCLUDE=src/nolfsrc
