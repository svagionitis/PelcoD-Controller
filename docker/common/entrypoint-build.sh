#!/usr/bin/env bash
# ==============================================================================
# PelcoD-Controller - Unified Inside-Container Build, Test & Package Entrypoint
#
# Supports Ubuntu, Fedora, Windows (cross), and FreeBSD (cross & native)
# container environments with selectable build modes:
#   BUILD_MODE = libs | libs-qt | tui | apps
# ==============================================================================
set -euo pipefail

# Allow direct command passthrough (e.g. `docker run -it <image> bash`)
if [ $# -gt 0 ]; then
    case "$1" in
        sh|bash|/bin/sh|/bin/bash)
            exec "$@"
            ;;
    esac
fi

# ------------------------------------------------------------------------------
# 1. Configuration & Environment Defaults
# ------------------------------------------------------------------------------
TARGET_OS="${TARGET_OS:-ubuntu}"
BUILD_MODE="${BUILD_MODE:-apps}"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
COMPILER_FAMILY="${COMPILER_FAMILY:-default}"
CMAKE_TARGET="${CMAKE_TARGET:-}"
RUN_TESTS="${RUN_TESTS:-false}"
CTEST_FILTER="${CTEST_FILTER:-}"
RUN_PACKAGE="${RUN_PACKAGE:-true}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
RECONFIGURE="${RECONFIGURE:-false}"
WARNINGS_AS_ERRORS="${WARNINGS_AS_ERRORS:-ON}"
ENABLE_HARDENING="${ENABLE_HARDENING:-ON}"
ENABLE_ONVIF="${ENABLE_ONVIF:-ON}"
ENABLE_VIDEO="${ENABLE_VIDEO:-ON}"
ENABLE_DJI="${ENABLE_DJI:-ON}"

if [ -z "${PARALLEL_JOBS:-}" ] || [ "${PARALLEL_JOBS}" -le 0 ] 2>/dev/null; then
    if command -v nproc >/dev/null 2>&1; then
        PARALLEL_JOBS="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        PARALLEL_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    else
        PARALLEL_JOBS=4
    fi
fi

WORKSPACE_DIR="${WORKSPACE_DIR:-/workspace}"
BUILD_ROOT="${BUILD_ROOT:-/build/${TARGET_OS}-${BUILD_MODE}-${BUILD_CONFIG}-${COMPILER_FAMILY}}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${WORKSPACE_DIR}/dist/${TARGET_OS}}"

export CCACHE_DIR="${CCACHE_DIR:-/cache/ccache}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

echo "======================================================================"
echo " PelcoD-Controller Container Build Runner"
echo "----------------------------------------------------------------------"
echo " Target OS          : ${TARGET_OS}"
echo " Build Mode         : ${BUILD_MODE}"
echo " Build Config       : ${BUILD_CONFIG}"
echo " Compiler Family    : ${COMPILER_FAMILY}"
echo " CMake Target       : ${CMAKE_TARGET:-<all>}"
echo " Parallel Jobs      : ${PARALLEL_JOBS}"
echo " Build Directory    : ${BUILD_ROOT}"
echo " Install Prefix     : ${INSTALL_PREFIX}"
echo " Run Tests          : ${RUN_TESTS}"
echo " Run Package        : ${RUN_PACKAGE}"
echo "======================================================================"

# ------------------------------------------------------------------------------
# 2. Validate Build Mode & Map CMake Options
# ------------------------------------------------------------------------------
case "${BUILD_MODE}" in
    libs)
        ENABLE_QT="OFF"
        BUILD_APPS="OFF"
        BUILD_TESTING_FLAG="ON"
        ;;
    libs-qt)
        ENABLE_QT="ON"
        BUILD_APPS="OFF"
        BUILD_TESTING_FLAG="ON"
        ;;
    tui)
        ENABLE_QT="OFF"
        BUILD_APPS="ON"
        BUILD_TESTING_FLAG="ON"
        ;;
    apps|all)
        ENABLE_QT="ON"
        BUILD_APPS="ON"
        BUILD_TESTING_FLAG="ON"
        ;;
    *)
        echo "[ERROR] Unsupported BUILD_MODE='${BUILD_MODE}' (expected: libs, libs-qt, tui, apps)" >&2
        exit 1
        ;;
esac

if [ "${RUN_TESTS}" = "false" ] && [ "${BUILD_TESTING:-}" = "OFF" ]; then
    BUILD_TESTING_FLAG="OFF"
fi

# ------------------------------------------------------------------------------
# 3. Prepare Directories & Offline Caches
# ------------------------------------------------------------------------------
if [ "${CLEAN_BUILD}" = "true" ] && [ -d "${BUILD_ROOT}" ]; then
    echo "[1/5] Cleaning build directory ${BUILD_ROOT} ..."
    rm -rf "${BUILD_ROOT}"
fi

mkdir -p "${BUILD_ROOT}" "${CCACHE_DIR}"

# Clean up any stale Ninja lock/restat files from interrupted runs
if [ -f "${BUILD_ROOT}/.ninja_log.restat" ]; then
    rm -f "${BUILD_ROOT}/.ninja_log.restat" 2>/dev/null || true
fi
if [ -f "${BUILD_ROOT}/.ninja_deps.recompact" ]; then
    rm -f "${BUILD_ROOT}/.ninja_deps.recompact" 2>/dev/null || true
fi

# Seed pre-fetched Live555 tarball into build directory for offline repeatability
if [ -f "/opt/cache/live555/live555-latest.tar.gz" ] && [ ! -f "${BUILD_ROOT}/_deps/live555/live555-latest.tar.gz" ]; then
    mkdir -p "${BUILD_ROOT}/_deps/live555"
    cp -f "/opt/cache/live555/live555-latest.tar.gz" "${BUILD_ROOT}/_deps/live555/live555-latest.tar.gz"
fi

# ------------------------------------------------------------------------------
# 4. CMake Configure
# ------------------------------------------------------------------------------
echo "[2/5] Configuring CMake (${BUILD_CONFIG}) ..."

CMAKE_ARGS=(
    -S "${WORKSPACE_DIR}"
    -B "${BUILD_ROOT}"
    -G Ninja
    -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}"
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
    -DPELCOD_ENABLE_QT="${ENABLE_QT}"
    -DPELCOD_BUILD_APPS="${BUILD_APPS}"
    -DPELCOD_ENABLE_ONVIF="${ENABLE_ONVIF}"
    -DPELCOD_ENABLE_VIDEO="${ENABLE_VIDEO}"
    -DPELCOD_ENABLE_DJI="${ENABLE_DJI}"
    -DBUILD_TESTING="${BUILD_TESTING_FLAG}"
    -DPELCOD_INSTALL_GIT_HOOKS=OFF
    -DWARNINGS_AS_ERRORS="${WARNINGS_AS_ERRORS}"
    -DENABLE_HARDENING="${ENABLE_HARDENING}"
)

if command -v ccache >/dev/null 2>&1; then
    CMAKE_ARGS+=(
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
fi

if [ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]; then
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}")
else
    case "${COMPILER_FAMILY}" in
        gcc|gnu)
            CMAKE_ARGS+=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
            ;;
        clang|llvm)
            CMAKE_ARGS+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
            ;;
        default|"")
            ;;
        *)
            CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER="${COMPILER_FAMILY}")
            ;;
    esac
fi

if [ -n "${QT_HOST_PATH:-}" ]; then
    CMAKE_ARGS+=(-DQT_HOST_PATH="${QT_HOST_PATH}")
fi

if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
    CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}")
fi

if [ -n "${CPACK_GENERATOR:-}" ]; then
    CMAKE_ARGS+=(-DCPACK_GENERATOR="${CPACK_GENERATOR}")
fi

if [ "${RECONFIGURE}" = "true" ] && [ -f "${BUILD_ROOT}/CMakeCache.txt" ]; then
    rm -f "${BUILD_ROOT}/CMakeCache.txt"
fi

cmake "${CMAKE_ARGS[@]}"

# ------------------------------------------------------------------------------
# 5. Compile Targets
# ------------------------------------------------------------------------------
echo "[3/5] Building targets (parallel jobs: ${PARALLEL_JOBS}) ..."

if [ -n "${CMAKE_TARGET}" ]; then
    # Allow space- or comma-separated target lists
    IFS=', ' read -r -a TARGET_LIST <<< "${CMAKE_TARGET}"
    cmake --build "${BUILD_ROOT}" --config "${BUILD_CONFIG}" --target "${TARGET_LIST[@]}" --parallel "${PARALLEL_JOBS}"
else
    cmake --build "${BUILD_ROOT}" --config "${BUILD_CONFIG}" --parallel "${PARALLEL_JOBS}"
fi

# ------------------------------------------------------------------------------
# 6. Execute Unit Tests (CTest)
# ------------------------------------------------------------------------------
if [ "${RUN_TESTS}" = "true" ]; then
    echo "[4/5] Running CTest suite ..."
    CTEST_ARGS=(
        --test-dir "${BUILD_ROOT}"
        --build-config "${BUILD_CONFIG}"
        --output-on-failure
        --parallel "${PARALLEL_JOBS}"
    )
    if [ -n "${CTEST_FILTER}" ]; then
        CTEST_ARGS+=(-R "${CTEST_FILTER}")
    fi

    if command -v xvfb-run >/dev/null 2>&1; then
        xvfb-run -a ctest "${CTEST_ARGS[@]}"
    else
        ctest "${CTEST_ARGS[@]}"
    fi
else
    echo "[4/5] Skipping unit tests (RUN_TESTS=false)."
fi

# ------------------------------------------------------------------------------
# 7. Install & Generate Redistributable CPack Archives
# ------------------------------------------------------------------------------
if [ "${RUN_PACKAGE}" = "true" ] && [ -z "${CMAKE_TARGET}" ]; then
    echo "[5/5] Installing artifacts and generating CPack packages into ${INSTALL_PREFIX} ..."
    mkdir -p "${INSTALL_PREFIX}" "${INSTALL_PREFIX}/packages"
    cmake --install "${BUILD_ROOT}" --config "${BUILD_CONFIG}" --prefix "${INSTALL_PREFIX}"

    (
        cd "${BUILD_ROOT}"
        cpack -C "${BUILD_CONFIG}" -B "${INSTALL_PREFIX}/packages"
    )

    # Restore non-root host ownership on installed artifacts if running as root
    if [ "$(id -u)" -eq 0 ] && [ -d "${INSTALL_PREFIX}" ]; then
        TARGET_UID="${HOST_UID:-$(stat -c '%u' "${WORKSPACE_DIR}" 2>/dev/null || echo 0)}"
        TARGET_GID="${HOST_GID:-$(stat -c '%g' "${WORKSPACE_DIR}" 2>/dev/null || echo 0)}"
        if [ "${TARGET_UID}" -ne 0 ] 2>/dev/null; then
            chown -R "${TARGET_UID}:${TARGET_GID}" "${INSTALL_PREFIX}" 2>/dev/null || true
        fi
    fi
else
    echo "[5/5] Skipping installation and packaging."
fi

echo "======================================================================"
echo " [OK] Container build completed successfully (${TARGET_OS} / ${BUILD_MODE})"
echo "======================================================================"

