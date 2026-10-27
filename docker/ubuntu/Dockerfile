# syntax=docker/dockerfile:1
# ==============================================================================
# PelcoD-Controller - Ubuntu 24.04 LTS (Noble) Build Container
#
# Builds all 20 C++17/Qt6 libraries, 9 GUI/QML/TUI applications, unit tests,
# and redistributable .deb / .tar.gz packages.
#
# Build context: docker/
#   docker build -t pelcod-builder:ubuntu-24.04 -f docker/ubuntu/Dockerfile docker/
# ==============================================================================
FROM ubuntu:24.04

LABEL org.opencontainers.image.title="PelcoD-Controller Ubuntu 24.04 Builder"
LABEL org.opencontainers.image.description="Hermetic build container for PelcoD-Controller libraries and apps on Ubuntu 24.04 LTS"

ENV DEBIAN_FRONTEND=noninteractive
ENV TARGET_OS=ubuntu
ENV QT_QPA_PLATFORM=offscreen
ENV CCACHE_DIR=/cache/ccache

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Compilers, Linkers & Build System
    build-essential \
    gcc-13 \
    g++-13 \
    clang \
    lld \
    llvm \
    clang-17 \
    lld-17 \
    llvm-17 \
    cmake \
    ninja-build \
    ccache \
    pkg-config \
    git \
    curl \
    ca-certificates \
    python3 \
    # Packaging & Headless Test Utilities
    dpkg-dev \
    file \
    xvfb \
    xauth \
    # Core, Logging & Unit Test Dependencies
    libgflags-dev \
    libgoogle-glog-dev \
    libgtest-dev \
    libgmock-dev \
    # ONVIF Profile S Dependencies
    libcurl4-openssl-dev \
    libssl-dev \
    libpugixml-dev \
    # Video & Filter Dependencies (FFmpeg, GStreamer + all plugins, OpenCV, OpenH264, V4L2)
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-good1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libgstrtspserver-1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-base-apps \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp \
    gstreamer1.0-tools \
    gstreamer1.0-gl \
    gstreamer1.0-x \
    gstreamer1.0-alsa \
    gstreamer1.0-pulseaudio \
    gstreamer1.0-pipewire \
    gstreamer1.0-vaapi \
    gstreamer1.0-opencv \
    gstreamer1.0-nice \
    libopencv-dev \
    libopenh264-dev \
    libv4l-dev \
    # Qt 6 Widgets, Quick, QML & Test Dependencies
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-declarative-dev \
    qt6-declarative-dev-tools \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-window \
    qml6-module-qtqml-workerscript \
    libgl1-mesa-dev \
    libxkbcommon-dev \
    && rm -rf /var/lib/apt/lists/*

# Pre-fetch and extract Live555 into /opt/cache/live555 for 100% offline builds
COPY common/fetch-live555.sh /usr/local/bin/fetch-live555.sh
RUN chmod +x /usr/local/bin/fetch-live555.sh && /usr/local/bin/fetch-live555.sh

# Install unified build/test/package entrypoint
COPY common/entrypoint-build.sh /usr/local/bin/entrypoint-build.sh
RUN chmod +x /usr/local/bin/entrypoint-build.sh \
    && mkdir -p /workspace /build /cache/ccache

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/entrypoint-build.sh"]

