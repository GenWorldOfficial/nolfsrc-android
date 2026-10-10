# No One Lives Forever, built from source.
#
#   make          build the whole game into out/game/ (and out/nolf-game.tar)
#   make test     scripted playtest of out/game/ on two levels
#   make progress how much of the build is byte-identical to the retail game;
#                 fails if a function recorded as identical stops matching
#   make help     everything else
#
# Needs only docker on the host, the two disc images in ISOS/ (ISOS/README.md)
# and the compilers in toolchain/ (`make toolchain` downloads them; see
# toolchain/README.md). Every step runs in a throwaway container of the image
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

TOOLCHAIN_FILES = toolchain/msvc600/VC98/Bin/CL.EXE \
	toolchain/msvc-6.00-8168/BIN/cl.exe \
	toolchain/msvc-6.00-8447/BIN/cl.exe \
	toolchain/icl-4.0/BIN/icl.exe

.PHONY: all game image toolchain check-isos check-toolchain test progress progress-update shell clean distclean help

all: game

help:
	@echo "make            build everything into out/game/ and out/nolf-game.tar"
	@echo "make toolchain  download and assemble the compilers in toolchain/ (once)"
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
	@echo "           make progress), COMPLETE=1, ACCEPT_LOSSES=1 (progress/README.md)"

game: check-isos check-toolchain image
	$(DOCKER_RUN) bash build/all.sh

image:
	@docker build -q -t $(IMAGE) docker/ >/dev/null || \
	  { echo "error: could not build the docker image (docker/Dockerfile)" >&2; exit 1; }
	@echo "== docker image $(IMAGE) ready"

toolchain: image
	$(DOCKER_RUN) bash toolchain/fetch.sh

check-isos:
	@NOLF_ISOS="$(ISODIR)" bash -c 'source build/isos.sh && iso_verify'

check-toolchain:
	@for f in $(TOOLCHAIN_FILES); do \
	  [ -f "$$f" ] || { echo "error: $$f is missing -- run make toolchain first (see toolchain/README.md)" >&2; exit 1; }; \
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
	$(DOCKER_RUN) env COMPLETE=$(COMPLETE) bash build/progress.sh check

progress-update: check-isos image
	$(DOCKER_RUN) env ACCEPT_LOSSES=$(ACCEPT_LOSSES) bash build/progress.sh update

shell: image
	docker run --rm -it --init --name $(CONTAINER)-shell \
	  -u $(shell id -u):$(shell id -g) -e HOME=/tmp/home \
	  -v $(CURDIR):/repo -w /repo $(ISO_MOUNTS) $(IMAGE) bash

clean:
	rm -rf out
	rm -rf src/engine/*/Release src/lithshared/*/Release src/lithshared/out \
	  src/game/*/Release src/game/*/Final_Release src/game/*/Debug \
	  src/engine/launcher/res
	rm -f src/engine/*/link/aliases.obj src/engine/*/link/vtables.obj \
	  src/engine/*/link/ddraw7.lib src/engine/*/link/mss32dec.lib \
	  src/engine/d3d/link/d3dviews.obj
	rm -f src/game/ObjectDLL/AnimationLex.cpp src/game/ObjectDLL/AnimationParse.cpp \
	  src/game/ObjectDLL/animationparse.cpp src/game/ObjectDLL/AnimationLexSymbols.h

distclean: clean
	-docker rmi $(IMAGE)
