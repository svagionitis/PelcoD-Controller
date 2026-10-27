#!/usr/bin/env bash
# ==============================================================================
# PelcoD-Controller - Live555 Tarball Pre-Fetcher & Validator for Build Images
#
# Executed during `docker build` to populate /opt/cache/live555 with both the
# verified tarball and pre-extracted sources so container builds run 100% offline.
# ==============================================================================
set -euo pipefail

CACHE_DIR="${LIVE555_CACHE_DIR:-/opt/cache/live555}"
TARBALL_PATH="${CACHE_DIR}/live555-latest.tar.gz"
EXTRACT_DIR="${CACHE_DIR}/src"

LIVE555_URLS=(
    "https://download.live555.com/live555-latest.tar.gz"
    "http://www.live555.com/liveMedia/public/live555-latest.tar.gz"
    "https://mirrors.videolan.org/pub/contrib/live555/live555-latest.tar.gz"
)

mkdir -p "${CACHE_DIR}" "${EXTRACT_DIR}"

download_succeeded=false
for url in "${LIVE555_URLS[@]}"; do
    echo "[fetch-live555] Attempting download from ${url} ..."
    if curl -fsSL --retry 3 --retry-delay 2 --connect-timeout 15 -o "${TARBALL_PATH}" "${url}"; then
        if [ -f "${TARBALL_PATH}" ]; then
            file_size=$(wc -c < "${TARBALL_PATH}" | tr -d ' ')
            if [ "${file_size}" -gt 100000 ] && tar -tzf "${TARBALL_PATH}" >/dev/null 2>&1; then
                echo "[fetch-live555] Verified valid tarball (${file_size} bytes) from ${url}"
                download_succeeded=true
                break
            fi
        fi
    fi
    rm -f "${TARBALL_PATH}"
done

if [ "${download_succeeded}" != "true" ]; then
    echo "[fetch-live555] WARNING: Could not pre-fetch Live555 tarball during image build." >&2
    exit 0
fi

tar -xzf "${TARBALL_PATH}" -C "${EXTRACT_DIR}"
chmod -R a+rX "${CACHE_DIR}"
echo "[fetch-live555] Cached Live555 archive and extracted sources in ${CACHE_DIR}"

