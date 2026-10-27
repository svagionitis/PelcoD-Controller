# syntax=docker/dockerfile:1
# ==============================================================================
# PelcoD-Controller - Fedora 41 Build Container
#
# Builds all 20 C++17/Qt6 libraries, 9 GUI/QML/TUI applications, unit tests,
# and redistributable .rpm / .tar.gz packages with GCC 14 and Clang 19.
#
# Build context: docker/
#   docker build -t pelcod-builder:fedora-41 -f docker/fedora/Dockerfile docker/
# ==============================================================================
FROM fedora:41

LABEL org.opencontainers.image.title="PelcoD-Controller Fedora 41 Builder"
LABEL org.opencontainers.image.description="Hermetic build container for PelcoD-Controller libraries and apps on Fedora 41"

ENV TARGET_OS=fedora
ENV QT_QPA_PLATFORM=offscreen
ENV CCACHE_DIR=/cache/ccache

RUN dnf install -y --enablerepo=fedora-cisco-openh264 \
    # Compilers, Linkers & Build System
    gcc \
    gcc-c++ \
    clang \
    lld \
    llvm \
    cmake \
    ninja-build \
    ccache \
    pkgconf-pkg-config \
    git \
    curl \
    ca-certificates \
    python3 \
    findutils \
    which \
    tar \
    gzip \
    # Packaging & Headless Test Utilities
    rpm-build \
    file \
    xorg-x11-server-Xvfb \
    xorg-x11-xauth \
    # Core, Logging & Unit Test Dependencies
    gflags-devel \
    glog-devel \
    gtest-devel \
    gmock-devel \
    # ONVIF Profile S Dependencies
    libcurl-devel \
    openssl-devel \
    pugixml-devel \
    # Video & Filter Dependencies (FFmpeg, GStreamer + all plugins, OpenCV, OpenH264, V4L2)
    ffmpeg-free-devel \
    gstreamer1 \
    gstreamer1-devel \
    gstreamer1-plugins-base \
    gstreamer1-plugins-base-devel \
    gstreamer1-plugins-base-tools \
    gstreamer1-plugins-good \
    gstreamer1-plugins-good-extras \
    gstreamer1-plugins-good-qt \
    gstreamer1-plugins-good-qt6 \
    gstreamer1-plugins-good-gtk \
    gstreamer1-plugins-bad-free \
    gstreamer1-plugins-bad-free-devel \
    gstreamer1-plugins-bad-free-extras \
    gstreamer1-plugins-ugly-free \
    gstreamer1-plugin-libav \
    gstreamer1-plugin-openh264 \
    gstreamer1-rtsp-server \
    gstreamer1-rtsp-server-devel \
    gstreamer1-vaapi \
    opencv-devel \
    openh264-devel \
    libv4l-devel \
    # Qt 6 Widgets, Quick, QML & Test Dependencies
    qt6-qtbase-devel \
    qt6-qtbase-gui \
    qt6-qtdeclarative-devel \
    mesa-libGL-devel \
    libxkbcommon-devel \
    && dnf clean all \
    && rm -rf /var/cache/dnf/*

# Pre-fetch and extract Live555 into /opt/cache/live555 for 100% offline builds
COPY common/fetch-live555.sh /usr/local/bin/fetch-live555.sh
RUN chmod +x /usr/local/bin/fetch-live555.sh && /usr/local/bin/fetch-live555.sh

# Install unified build/test/package entrypoint
COPY common/entrypoint-build.sh /usr/local/bin/entrypoint-build.sh
RUN chmod +x /usr/local/bin/entrypoint-build.sh \
    && mkdir -p /workspace /build /cache/ccache

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/entrypoint-build.sh"]

