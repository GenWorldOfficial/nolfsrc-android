# No One Lives Forever, built from source.
#
#   make          build the whole game into out/game/ (and out/nolf-game.tar)
#   make test     scripted playtest of out/game/ on two levels
#   make debug    the same game with symbols and debug logging, in out/game-debug/
#   make modern   the game with the retail bugs fixed (NOLFSRC_MODERN), in out/game-modern/
#   make progress how much of the build is byte-identical to the retail game;
#                 fails if a function recorded as identical stops matching
#   make help     everything else
#
# Needs only docker on the host, the two disc images in ISOS/ (ISOS/README.md)
# and the compilers, the toolchains/ submodule (`git submodule update --init`;
# see toolchains/README.md). Every step runs in a throwaway container of the image
# docker/Dockerfile describes, with this checkout mounted at /repo, as you.

IMAGE     ?= nolfsrc-build
CONTAINER ?= nolfsrc-build
ISODIR    ?= $(if $(NOLF_ISOS),$(NOLF_ISOS),$(CURDIR)/ISOS)
TEST_LEVELS ?= T01S01 M03S01

# The disc images are mounted by their real paths, so ISOS/ may hold symlinks.
ISO1 := $(realpath $(ISODIR)/NOLF1-CD1.iso)
ISO2 := $(realpath $(ISODIR)/NOLF1-CD2.iso)
ISO_MOUNTS := $(if $(ISO1),-v $(ISO1):/isos/NOLF1-CD1.iso:ro) $(if $(ISO2),-v $(ISO2):/isos/NOLF1-CD2.iso:ro) -e NOLF_ISOS=/isos

# The NOLF Source Code v1.003 release, for scoring the lithshared libraries
# against its prebuilt .libs (progress/README.md). Optional.
NOLF_SOURCE ?=
SRC_MOUNT := $(if $(NOLF_SOURCE),-v $(realpath $(NOLF_SOURCE)):/nolfsource:ro -e NOLF_SOURCE=/nolfsource)

DOCKER_RUN = docker run --rm --init --name $(CONTAINER) \
	-u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	-v $(CURDIR):/repo -w /repo $(ISO_MOUNTS) $(SRC_MOUNT) $(IMAGE)

TOOLCHAIN_FILES = toolchains/msvc-6.00-9782/VC98/Bin/CL.EXE \
	toolchains/msvc-6.00-8168/BIN/cl.exe \
	toolchains/msvc-6.00-8447/BIN/cl.exe \
	toolchains/icl-4.0/BIN/icl.exe

.PHONY: all game debug modern modern-debug image check-isos check-toolchain test progress progress-update shell clean distclean help

all: game

help:
	@echo "make            build everything into out/game/ and out/nolf-game.tar"
	@echo "make debug      the game with symbols and NOLFSRC_DEBUG logging, in out/game-debug/"
	@echo "make modern     the game with NOLFSRC_MODERN (the retail bugs fixed), in out/game-modern/"
	@echo "make modern-debug  both, in out/game-modern-debug/"
	@echo "                (none of these is ever scored; build/variant.sh)"
	@echo "make test       scripted playtest of out/game/ ($(TEST_LEVELS)), under Xvfb"
	@echo "make progress   score the build against the retail binaries; fails if a"
	@echo "                function recorded in progress/identical.txt stops matching"
	@echo "make progress-update  record the score in progress/ and README.md"
	@echo "make shell      a shell in the build container (build/*.sh, tools/ work there)"
	@echo "make image      (re)build the docker image $(IMAGE)"
	@echo "make clean      remove out/ and every build output in the source tree"
	@echo "make distclean  clean, and remove the docker image too"
	@echo
	@echo "Variables: NOLF_ISOS=<dir> (default ISOS/), TEST_LEVELS=\"$(TEST_LEVELS)\""
	@echo "           NOLF_SOURCE=<dir> (the NOLF Source Code v1.003 release, for"
	@echo "           make progress), COMPLETE=1, COVERED=1, ACCEPT_LOSSES=1 (progress/README.md)"

game: check-isos check-toolchain image
	$(DOCKER_RUN) bash build/all.sh

debug modern modern-debug: check-isos check-toolchain image
	$(DOCKER_RUN) bash build/variant.sh $@

image:
	@docker build -q -t $(IMAGE) docker/ >/dev/null || \
	  { echo "error: could not build the docker image (docker/Dockerfile)" >&2; exit 1; }
	@echo "== docker image $(IMAGE) ready"

check-isos:
	@NOLF_ISOS="$(ISODIR)" bash -c 'source build/isos.sh && iso_verify'

check-toolchain:
	@for f in $(TOOLCHAIN_FILES); do \
	  [ -f "$$f" ] || { echo "error: $$f is missing -- the compilers are the toolchains/ submodule: run git submodule update --init (see toolchains/README.md)" >&2; exit 1; }; \
	done

test: image
	@[ -f out/game/lithtech.exe ] || { echo "error: no out/game/ -- run make first" >&2; exit 1; }
	@rc=0; for w in $(TEST_LEVELS); do \
	  echo "== playtest: Worlds\\$$w (tools/playtest-basic.txt)"; \
	  $(DOCKER_RUN) env PLAYTEST_SCRIPT=tools/playtest-basic.txt PLAYTEST_NAME=test-$$w \
	    bash tools/playtest.sh out/game "Worlds\\$$w" || rc=1; \
	done; \
	if [ $$rc = 0 ]; then echo "== make test: every level ALIVE (shots in out/play/test-*/)"; \
	else echo "== make test: FAILED (see out/play/test-*/result.txt, wine.log)"; fi; exit $$rc

progress: check-isos image
	$(DOCKER_RUN) env COMPLETE=$(COMPLETE) COVERED=$(COVERED) bash build/progress.sh check

progress-update: check-isos image
	$(DOCKER_RUN) env ACCEPT_LOSSES=$(ACCEPT_LOSSES) bash build/progress.sh update

shell: image
	docker run --rm -it --init --name $(CONTAINER)-shell \
	  -u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	  -v $(CURDIR):/repo -w /repo $(ISO_MOUNTS) $(SRC_MOUNT) $(IMAGE) bash

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
