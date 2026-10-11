# The build image: Ubuntu 24.04 with its distro Wine (32-bit), Python 3, and
# what the scripted playtest needs (Xvfb, xdotool, xwd, ImageMagick, Mesa),
# what toolchains/fetch.sh needs to rebuild the compiler trees from their
# original media (curl, bsdtar, 7z, cabextract, unar, unshield), and objdump
# (binutils), which the reconstruction tools disassemble with.
#
# The Makefile builds it as `nolfsrc-build` and runs every step in a
# throwaway container with the checkout mounted at /repo, as the invoking
# user. Nothing from the repository, the toolchain or the disc images is
# baked into the image.
#
# Why a container at all: Wine is the only way to run the 2000-era
# compilers, and Ubuntu 24.04's Wine 9.0 is the one this build is known to
# work with -- some newer host userspaces break every current Wine release.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive PYTHONDONTWRITEBYTECODE=1
RUN dpkg --add-architecture i386 \
 && apt-get update -qq \
 && apt-get install -y -qq \
        wine wine32:i386 \
        python3 binutils \
        xvfb xauth xdotool x11-apps imagemagick \
        libgl1:i386 libglu1-mesa:i386 libgl1-mesa-dri:i386 \
        procps \
        curl ca-certificates libarchive-tools p7zip-full cabextract unar unshield \
 && rm -rf /var/lib/apt/lists/*

COPY entry.sh /usr/local/bin/nolf-entry
RUN chmod 755 /usr/local/bin/nolf-entry
ENTRYPOINT ["/usr/local/bin/nolf-entry"]
CMD ["bash"]
