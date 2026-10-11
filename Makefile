# No One Lives Forever, built from source.
#
#   make          build the whole game into out/nolf-legacy-release/ (and out/nolf-legacy-release.tar)
#   make test     scripted playtest of out/nolf-legacy-release/ on two levels
#   make debug    the same game with symbols and debug logging, in out/nolf-legacy-debug/
#   make modern   the game with the retail bugs fixed (NOLFSRC_MODERN), in out/nolf-modern-release/
#   make progress how much of the build is byte-identical to the retail game;
#                 fails if a function recorded as identical stops matching
#   make help     everything else
#
# Needs only docker on the host and the two submodules (`git submodule update
# --init`): the compilers, toolchains/ (toolchains/README.md), and rez/, which
# fetches the redump images of the Game of the Year discs from archive.org and
# installs the game from them (rez/README.md; `make retail`, which every build
# runs first, does it once). Every step runs in a throwaway container of the
# image docker/Dockerfile describes, with this checkout mounted at /repo, as
# you.

IMAGE     ?= nolfsrc-build
CONTAINER ?= nolfsrc-build
TEST_LEVELS ?= T01S01 M03S01

# The NOLF Source Code v1.003 release, for scoring the lithshared libraries
# against its prebuilt .libs (progress/README.md). Optional.
NOLF_SOURCE ?=
SRC_MOUNT := $(if $(NOLF_SOURCE),-v $(realpath $(NOLF_SOURCE)):/nolfsource:ro -e NOLF_SOURCE=/nolfsource)

DOCKER_RUN = docker run --rm --init --name $(CONTAINER) \
	-u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	-v $(CURDIR):/repo -w /repo $(SRC_MOUNT) $(IMAGE)

TOOLCHAIN_FILES = toolchains/msvc-6.00-9782/VC98/Bin/CL.EXE \
	toolchains/msvc-6.00-8168/BIN/cl.exe \
	toolchains/msvc-6.00-8447/BIN/cl.exe \
	toolchains/icl-4.0/BIN/icl.exe

.PHONY: all game debug modern modern-debug image retail check-rez check-toolchain test progress progress-update shell clean distclean help

all: game

help:
	@echo "make            build everything into out/nolf-legacy-release/ and out/nolf-legacy-release.tar"
	@echo "make debug      the game with symbols and NOLFSRC_DEBUG logging, in out/nolf-legacy-debug/"
	@echo "make modern     the game with NOLFSRC_MODERN (the retail bugs fixed), in out/nolf-modern-release/"
	@echo "make modern-debug  both, in out/nolf-modern-debug/"
	@echo "                (none of these is ever scored; build/variant.sh)"
	@echo "make test       scripted playtest of out/nolf-legacy-release/ ($(TEST_LEVELS)), under Xvfb"
	@echo "make progress   score the build against the retail binaries; fails if a"
	@echo "                function recorded in progress/identical.txt stops matching"
	@echo "make progress-update  record the score in progress/ and README.md"
	@echo "make retail     fetch the two disc images and install the retail game from"
	@echo "                them (rez/, once; every build runs it), and check it"
	@echo "make shell      a shell in the build container (build/*.sh, tools/ work there)"
	@echo "make image      (re)build the docker image $(IMAGE)"
	@echo "make clean      remove out/ and every build output in the source tree"
	@echo "make distclean  clean, and remove the docker image too"
	@echo
	@echo "Variables: TEST_LEVELS=\"$(TEST_LEVELS)\""
	@echo "           NOLF_SOURCE=<dir> (the NOLF Source Code v1.003 release, for"
	@echo "           make progress), COMPLETE=1, COVERED=1, ACCEPT_LOSSES=1 (progress/README.md)"

game: check-toolchain retail
	$(DOCKER_RUN) bash build/all.sh

debug modern modern-debug: check-toolchain retail
	$(DOCKER_RUN) bash build/variant.sh $@

image:
	@docker build -q -t $(IMAGE) docker/ >/dev/null || \
	  { echo "error: could not build the docker image (docker/Dockerfile)" >&2; exit 1; }
	@echo "== docker image $(IMAGE) ready"

# The retail game the build takes its data from (build/retail.sh): installed
# by rez/ the first time, checked against rez/games.sha256 every time.
retail: check-rez image
	@$(DOCKER_RUN) bash -c 'source build/retail.sh && retail_install && retail_verify'

check-rez:
	@[ -f rez/install-games.sh ] || { echo "error: rez/ is empty -- the disc images and their installer are the rez/ submodule: run git submodule update --init (see rez/README.md)" >&2; exit 1; }

check-toolchain:
	@for f in $(TOOLCHAIN_FILES); do \
	  [ -f "$$f" ] || { echo "error: $$f is missing -- the compilers are the toolchains/ submodule: run git submodule update --init (see toolchains/README.md)" >&2; exit 1; }; \
	done

test: image
	@[ -f out/nolf-legacy-release/lithtech.exe ] || { echo "error: no out/nolf-legacy-release/ -- run make first" >&2; exit 1; }
	@rc=0; for w in $(TEST_LEVELS); do \
	  echo "== playtest: Worlds\\$$w (tools/playtest-basic.txt)"; \
	  $(DOCKER_RUN) env PLAYTEST_SCRIPT=tools/playtest-basic.txt PLAYTEST_NAME=test-$$w \
	    bash tools/playtest.sh out/nolf-legacy-release "Worlds\\$$w" || rc=1; \
	done; \
	if [ $$rc = 0 ]; then echo "== make test: every level ALIVE (shots in out/play/test-*/)"; \
	else echo "== make test: FAILED (see out/play/test-*/result.txt, wine.log)"; fi; exit $$rc

progress: retail
	$(DOCKER_RUN) env COMPLETE=$(COMPLETE) COVERED=$(COVERED) bash build/progress.sh check

progress-update: retail
	$(DOCKER_RUN) env ACCEPT_LOSSES=$(ACCEPT_LOSSES) bash build/progress.sh update

shell: image
	docker run --rm -it --init --name $(CONTAINER)-shell \
	  -u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	  -v $(CURDIR):/repo -w /repo $(SRC_MOUNT) $(IMAGE) bash

clean:
	rm -rf out
	rm -rf src/engine/*/Release src/lithshared/*/Release src/lithshared/*/Debug src/lithshared/out \
	  src/game/*/Release src/game/*/Final_Release src/game/*/Debug \
	  src/engine/launcher/res src/game/ClientRes/goty
	rm -f src/engine/*/link/aliases.obj src/engine/*/link/vtables.obj \
	  src/engine/*/link/ddraw7.lib src/engine/*/link/mss32dec.lib \
	  src/engine/d3d/link/d3dviews.obj
	rm -f src/game/ObjectDLL/AnimationLex.cpp src/game/ObjectDLL/AnimationParse.cpp \
	  src/game/ObjectDLL/animationparse.cpp src/game/ObjectDLL/AnimationLexSymbols.h

distclean: clean
	-docker rmi $(IMAGE)
