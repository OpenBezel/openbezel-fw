# OpenBezel build environment: everything the build and the tools need, from Ubuntu's
# archive.  The release is pinned so SDCC, Pillow/FreeType and the font packages - and so
# the firmware image and the rendered glyphs - come out byte for byte the same everywhere.
# Use it through tools/docker-run.
FROM ubuntu:26.04

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      sdcc meson ninja-build git ca-certificates \
      python3 python3-pil python3-tk python3-kconfiglib python3-smbus2 python3-numpy \
      librsvg2-bin edid-decode \
      fonts-dejavu-core fonts-inter \
      gcc libc6-dev \
 && rm -rf /var/lib/apt/lists/*

# tools/docker-run runs as the calling user, with HOME here
ENV HOME=/tmp/home
RUN mkdir -m 1777 /tmp/home
