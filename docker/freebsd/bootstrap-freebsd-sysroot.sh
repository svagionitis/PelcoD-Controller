#!/usr/bin/env bash
# ==============================================================================
# PelcoD-Controller - FreeBSD 14.2-RELEASE Sysroot & Package Bootstrapper
#
# Runs on a Linux build host (inside docker/freebsd/Dockerfile.cross) to
# construct a self-contained FreeBSD 14.2 amd64 sysroot at /opt/freebsd-sysroot:
#   1. Downloads & extracts official FreeBSD 14.2-RELEASE base.txz
#   2. Downloads & parses official FreeBSD 14:amd64 packagesite.pkg metadata
#   3. Resolves transitive dependencies and extracts official FreeBSD binary
#      packages (gflags, glog, googletest, curl, pugixml, openh264, liveMedia,
#      ffmpeg, gstreamer1, opencv, qt6-base, qt6-declarative)
#   4. Normalizes absolute symlinks, linker scripts, and CMake/pkg-config paths
#      so Linux clang++-18 + ld.lld-18 cross-compile cleanly against the sysroot.
# ==============================================================================
set -euo pipefail

FREEBSD_VERSION="${FREEBSD_VERSION:-14.2-RELEASE}"
FREEBSD_ABI="${FREEBSD_ABI:-FreeBSD:14:amd64}"
FREEBSD_PKG_BRANCH="${FREEBSD_PKG_BRANCH:-quarterly}"
SYSROOT_DIR="${SYSROOT_DIR:-/opt/freebsd-sysroot}"
INSTALL_GUI_AND_VIDEO_PKGS="${INSTALL_GUI_AND_VIDEO_PKGS:-ON}"

BASE_URL="https://download.freebsd.org/releases/amd64/${FREEBSD_VERSION}/base.txz"
PKG_REPO_URL="https://pkg.freebsd.org/${FREEBSD_ABI}/${FREEBSD_PKG_BRANCH}"
TMP_DIR="/tmp/freebsd-bootstrap"

echo "======================================================================"
echo " Bootstrapping FreeBSD ${FREEBSD_VERSION} Sysroot"
echo "----------------------------------------------------------------------"
echo " Target Sysroot : ${SYSROOT_DIR}"
echo " Base Archive   : ${BASE_URL}"
echo " Package Repo   : ${PKG_REPO_URL}"
echo " GUI/Video Pkgs : ${INSTALL_GUI_AND_VIDEO_PKGS}"
echo "======================================================================"

mkdir -p "${SYSROOT_DIR}" "${TMP_DIR}"

# ------------------------------------------------------------------------------
# 1. Download and Extract FreeBSD 14.2-RELEASE base.txz
# ------------------------------------------------------------------------------
echo "[1/4] Downloading and extracting FreeBSD ${FREEBSD_VERSION} base.txz ..."
curl -fL --retry 3 --retry-delay 5 "${BASE_URL}" -o "${TMP_DIR}/base.txz"

# Extract only headers, libraries, libdata, and runtime objects required for linking
tar -xJf "${TMP_DIR}/base.txz" -C "${SYSROOT_DIR}" \
    ./lib \
    ./usr/lib \
    ./usr/libdata \
    ./usr/include \
    2>/dev/null || tar -xJf "${TMP_DIR}/base.txz" -C "${SYSROOT_DIR}" --exclude='./rescue' --exclude='./usr/tests' --exclude='./usr/share/man'

rm -f "${TMP_DIR}/base.txz"

# ------------------------------------------------------------------------------
# 2. Resolve and Extract Official FreeBSD Binary Packages
# ------------------------------------------------------------------------------
echo "[2/4] Fetching FreeBSD package index (${PKG_REPO_URL}/packagesite.pkg) ..."
if ! curl -fL --retry 3 "${PKG_REPO_URL}/packagesite.pkg" -o "${TMP_DIR}/packagesite.pkg"; then
    curl -fL --retry 3 "${PKG_REPO_URL}/packagesite.txz" -o "${TMP_DIR}/packagesite.pkg"
fi

mkdir -p "${TMP_DIR}/pkgmeta"
tar -xf "${TMP_DIR}/packagesite.pkg" -C "${TMP_DIR}/pkgmeta"

ROOT_PACKAGES=(
    gflags
    glog
    googletest
    curl
    pugixml
    openh264
    liveMedia
)

if [ "${INSTALL_GUI_AND_VIDEO_PKGS}" = "ON" ]; then
    ROOT_PACKAGES+=(
        ffmpeg
        gstreamer1
        gstreamer1-plugins-base
        gstreamer1-plugins-good
        gstreamer1-plugins-bad
        gstreamer1-rtsp-server
        opencv
        qt6-base
        qt6-declarative
    )
fi

echo "[3/4] Resolving transitive dependencies and extracting packages ..."
python3 - "${TMP_DIR}/pkgmeta/packagesite.yaml" "${PKG_REPO_URL}" "${SYSROOT_DIR}" "${TMP_DIR}" "${ROOT_PACKAGES[@]}" << 'PYEOF'
import json
import os
import subprocess
import sys
import urllib.request

packagesite_path = sys.argv[1]
repo_url = sys.argv[2]
sysroot_dir = sys.argv[3]
tmp_dir = sys.argv[4]
requested = sys.argv[5:]

pkg_index = {}
with open(packagesite_path, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
            name = obj.get("name")
            if name:
                pkg_index[name] = obj
        except json.JSONDecodeError:
            continue

resolved_order = []
visited = set()

def resolve(pkg_name):
    if pkg_name in visited:
        return
    visited.add(pkg_name)
    meta = pkg_index.get(pkg_name)
    if not meta:
        print(f"[WARN] Package '{pkg_name}' not found in FreeBSD packagesite index; skipping.")
        return
    deps = meta.get("deps", {})
    if isinstance(deps, dict):
        for dep_name in sorted(deps.keys()):
            resolve(dep_name)
    resolved_order.append(pkg_name)

for root_pkg in requested:
    resolve(root_pkg)

print(f"[INFO] Resolved {len(resolved_order)} total FreeBSD packages for sysroot.")

pkg_dl_dir = os.path.join(tmp_dir, "pkgs")
os.makedirs(pkg_dl_dir, exist_ok=True)

for idx, pkg_name in enumerate(resolved_order, 1):
    meta = pkg_index[pkg_name]
    repopath = meta["repopath"]
    pkg_url = f"{repo_url}/{repopath}"
    local_pkg = os.path.join(pkg_dl_dir, os.path.basename(repopath))
    print(f"  [{idx}/{len(resolved_order)}] {pkg_name}-{meta.get('version', '')} <- {repopath}")
    urllib.request.urlretrieve(pkg_url, local_pkg)
    subprocess.run(
        [
            "tar",
            "-xf",
            local_pkg,
            "-C",
            sysroot_dir,
            "--exclude=+*",
            "--exclude=usr/local/man",
            "--exclude=usr/local/share/doc",
            "--exclude=usr/local/share/examples",
        ],
        check=True,
    )
    os.remove(local_pkg)
PYEOF

# ------------------------------------------------------------------------------
# 3. Normalize Absolute Symlinks, Linker Scripts, and CMake/PkgConfig Paths
# ------------------------------------------------------------------------------
echo "[4/4] Normalizing symlinks, linker scripts, and CMake paths in ${SYSROOT_DIR} ..."
python3 - "${SYSROOT_DIR}" << 'PYEOF'
import os
import re
import sys

sysroot = os.path.abspath(sys.argv[1])

# 1. Convert absolute symlinks pointing to /lib/... or /usr/... into relative symlinks
for root, dirs, files in os.walk(sysroot):
    for entry in dirs + files:
        full_path = os.path.join(root, entry)
        if os.path.islink(full_path):
            target = os.readlink(full_path)
            if target.startswith("/"):
                target_in_sysroot = os.path.join(sysroot, target.lstrip("/"))
                rel_target = os.path.relpath(target_in_sysroot, os.path.dirname(full_path))
                os.unlink(full_path)
                os.symlink(rel_target, full_path)

# 2. Ensure CMake config files under usr/local/lib/cmake reference the sysroot prefix
cmake_root = os.path.join(sysroot, "usr", "local", "lib", "cmake")
if os.path.isdir(cmake_root):
    for root, _, files in os.walk(cmake_root):
        for fname in files:
            if not fname.endswith(".cmake"):
                continue
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, "r", encoding="utf-8") as f:
                    content = f.read()
            except (OSError, UnicodeDecodeError):
                continue
            updated = re.sub(
                r'(?<!/opt/freebsd-sysroot)(/usr/local/(?:include|lib|libdata|share|bin|libexec))',
                sysroot + r'\1',
                content,
            )
            if updated != content:
                with open(fpath, "w", encoding="utf-8") as f:
                    f.write(updated)
PYEOF

rm -rf "${TMP_DIR}"
echo "[OK] FreeBSD ${FREEBSD_VERSION} sysroot ready at ${SYSROOT_DIR}."

