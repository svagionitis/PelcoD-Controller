#!/usr/bin/env bash
# ==============================================================================
# PelcoD-Controller - Multi-OS Containerized Build & Packaging Orchestrator
#
# Builds PelcoD-Controller libraries, TUI tools, Qt 6 GUI/QML applications,
# unit tests, and redistributable packages across Ubuntu 24.04, Fedora 41,
# Windows x64, and FreeBSD 14.2 inside isolated OCI/Docker containers.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKER_DIR="${REPO_ROOT}/docker"
COMPOSE_FILE="${DOCKER_DIR}/docker-compose.yml"

# ------------------------------------------------------------------------------
# Default Configuration
# ------------------------------------------------------------------------------
OS_LIST_RAW="all"
BUILD_MODE="${BUILD_MODE:-apps}"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
COMPILER_FAMILY="${COMPILER_FAMILY:-default}"
CMAKE_TARGET="${CMAKE_TARGET:-}"
RUN_TESTS="${RUN_TESTS:-false}"
RUN_TESTS_EXPLICIT="false"
CTEST_FILTER="${CTEST_FILTER:-}"
RUN_PACKAGE="${RUN_PACKAGE:-true}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
RECONFIGURE="${RECONFIGURE:-false}"
WARNINGS_AS_ERRORS="${WARNINGS_AS_ERRORS:-ON}"
ENABLE_HARDENING="${ENABLE_HARDENING:-ON}"
ENABLE_ONVIF="${ENABLE_ONVIF:-ON}"
ENABLE_VIDEO="${ENABLE_VIDEO:-ON}"
ENABLE_DJI="${ENABLE_DJI:-ON}"
REBUILD_IMAGE="false"
BUILD_IMAGE_ONLY="false"
INTERACTIVE_SHELL="false"

if [ -z "${PARALLEL_JOBS:-}" ]; then
    if command -v nproc >/dev/null 2>&1; then
        PARALLEL_JOBS="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        PARALLEL_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    else
        PARALLEL_JOBS=4
    fi
fi

# ------------------------------------------------------------------------------
# Usage / Help
# ------------------------------------------------------------------------------
print_usage() {
    cat << 'EOF'
Usage:
    ./scripts/container_build.sh [OPTIONS]

Options:
    --os <list>           Target OS list (comma-separated) or "all":
                          ubuntu, fedora, windows, freebsd, freebsd-vm
                          (default: all -> ubuntu,fedora,windows,freebsd)
    --mode <mode>         Build scope:
                          "libs"    - Core C++17 libraries only (no Qt, no apps)
                          "libs-qt" - Core + Qt 6 adapter libraries (no apps)
                          "tui"     - Core libraries + PelcoDAppTui + OnvifServerApp
                          "apps"    - All libraries + all Qt 6 GUI/QML & TUI apps (default)
    --config <cfg>        Build configuration:
                          Release (default), Debug, RelWithDebInfo, MinSizeRel
    --compiler <comp>     Compiler family inside the container:
                          default, gcc, clang, clang-cl, mingw
    --target <name>       Build only a specific CMake target (or comma-separated list),
                          e.g. PelcoDCore, Onvif, SightlineAppQt
    --run-tests           Execute the CTest unit test suite inside the container
    --ctest-filter <reg>  Pass -R <regex> to CTest when --run-tests is enabled
    --no-package          Skip cmake --install and CPack archive generation
    --clean               Wipe the container build directory before configuring
    --reconfigure         Remove CMakeCache.txt and force a fresh CMake configure step
    --rebuild-image       Force rebuilding the Docker image(s) before running
    --build-image-only    Build the Docker image(s) for the selected --os and exit
    --shell               Launch an interactive bash shell inside the target container
    -j, --jobs <N>        Number of parallel build/test jobs inside the container
    -h, --help            Show this help message and exit

Examples:
    # Build all libraries, GUI/QML apps, TUI apps, and packages for all 4 OSes:
    ./scripts/container_build.sh --os all --mode apps

    # Build only core C++17 libraries (zero Qt dependency) on Ubuntu and Fedora:
    ./scripts/container_build.sh --os ubuntu,fedora --mode libs --run-tests

    # Cross-compile Windows x64 libraries, Qt 6 GUI apps, and .zip bundle on Linux:
    ./scripts/container_build.sh --os windows --mode apps

    # Cross-compile FreeBSD 14.2 ELF64 libraries, Qt 6 GUI apps, and .tar.gz bundle:
    ./scripts/container_build.sh --os freebsd --mode apps

    # Build a single library target (e.g. Onvif) across all 4 OSes:
    ./scripts/container_build.sh --os all --mode libs --target Onvif --no-package
EOF
}

# ------------------------------------------------------------------------------
# Parse Command-Line Arguments
# ------------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --os)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --os requires an argument." >&2
                exit 1
            fi
            OS_LIST_RAW="$2"
            shift 2
            ;;
        --os=*)
            OS_LIST_RAW="${1#*=}"
            shift
            ;;
        --mode)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --mode requires an argument." >&2
                exit 1
            fi
            BUILD_MODE="$2"
            shift 2
            ;;
        --mode=*)
            BUILD_MODE="${1#*=}"
            shift
            ;;
        --config)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --config requires an argument." >&2
                exit 1
            fi
            BUILD_CONFIG="$2"
            shift 2
            ;;
        --config=*)
            BUILD_CONFIG="${1#*=}"
            shift
            ;;
        --compiler)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --compiler requires an argument." >&2
                exit 1
            fi
            COMPILER_FAMILY="$2"
            shift 2
            ;;
        --compiler=*)
            COMPILER_FAMILY="${1#*=}"
            shift
            ;;
        --target)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --target requires an argument." >&2
                exit 1
            fi
            CMAKE_TARGET="$2"
            shift 2
            ;;
        --target=*)
            CMAKE_TARGET="${1#*=}"
            shift
            ;;
        --run-tests)
            RUN_TESTS="true"
            RUN_TESTS_EXPLICIT="true"
            shift
            ;;
        --ctest-filter)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --ctest-filter requires a regex argument." >&2
                exit 1
            fi
            CTEST_FILTER="$2"
            shift 2
            ;;
        --ctest-filter=*)
            CTEST_FILTER="${1#*=}"
            shift
            ;;
        --no-package)
            RUN_PACKAGE="false"
            shift
            ;;
        --clean)
            CLEAN_BUILD="true"
            shift
            ;;
        --reconfigure)
            RECONFIGURE="true"
            shift
            ;;
        --rebuild-image)
            REBUILD_IMAGE="true"
            shift
            ;;
        --build-image-only)
            BUILD_IMAGE_ONLY="true"
            shift
            ;;
        --shell)
            INTERACTIVE_SHELL="true"
            shift
            ;;
        -j|--jobs)
            if [ $# -lt 2 ]; then
                echo "[ERROR] $1 requires a numeric job count." >&2
                exit 1
            fi
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -j*)
            PARALLEL_JOBS="${1#-j}"
            shift
            ;;
        --jobs=*)
            PARALLEL_JOBS="${1#*=}"
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown argument: '$1'" >&2
            print_usage >&2
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Validate Options
# ------------------------------------------------------------------------------
case "${BUILD_MODE}" in
    libs|libs-qt|tui|apps|all)
        ;;
    *)
        echo "[ERROR] Invalid --mode '${BUILD_MODE}'. Expected: libs, libs-qt, tui, apps." >&2
        exit 1
        ;;
esac

case "${BUILD_CONFIG}" in
    Release|Debug|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        echo "[ERROR] Invalid --config '${BUILD_CONFIG}'. Expected: Release, Debug, RelWithDebInfo, MinSizeRel." >&2
        exit 1
        ;;
esac

if ! [[ "${PARALLEL_JOBS}" =~ ^[0-9]+$ ]] || [ "${PARALLEL_JOBS}" -le 0 ]; then
    echo "[ERROR] Invalid job count '${PARALLEL_JOBS}'. Expected a positive integer." >&2
    exit 1
fi

if [ "${OS_LIST_RAW}" = "all" ]; then
    OS_TARGETS=(ubuntu fedora windows freebsd)
else
    IFS=',' read -r -a OS_TARGETS <<< "${OS_LIST_RAW}"
fi

if [ "${#OS_TARGETS[@]}" -eq 0 ]; then
    echo "[ERROR] No target OS specified." >&2
    exit 1
fi

for os_item in "${OS_TARGETS[@]}"; do
    case "${os_item}" in
        ubuntu|fedora|windows|freebsd|freebsd-vm)
            ;;
        *)
            echo "[ERROR] Unsupported OS target '${os_item}'. Expected: ubuntu, fedora, windows, freebsd, freebsd-vm, or all." >&2
            exit 1
            ;;
    esac
done

if [ "${INTERACTIVE_SHELL}" = "true" ] && [ "${#OS_TARGETS[@]}" -gt 1 ]; then
    echo "[ERROR] --shell can only be used with a single --os target (got: ${OS_LIST_RAW})." >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# Detect Container Runtime & Compose Capability
# ------------------------------------------------------------------------------
CONTAINER_ENGINE=""
COMPOSE_CMD=()

if command -v docker >/dev/null 2>&1; then
    CONTAINER_ENGINE="docker"
    if docker compose version >/dev/null 2>&1; then
        COMPOSE_CMD=(docker compose -f "${COMPOSE_FILE}")
    elif command -v docker-compose >/dev/null 2>&1; then
        COMPOSE_CMD=(docker-compose -f "${COMPOSE_FILE}")
    fi
elif command -v podman >/dev/null 2>&1; then
    CONTAINER_ENGINE="podman"
    if podman compose version >/dev/null 2>&1; then
        COMPOSE_CMD=(podman compose -f "${COMPOSE_FILE}")
    fi
else
    echo "[ERROR] Neither 'docker' nor 'podman' was found in PATH." >&2
    exit 1
fi

HOST_UID="$(id -u)"
HOST_GID="$(id -g)"
export HOST_UID HOST_GID

# ------------------------------------------------------------------------------
# Helper Functions for Service Metadata
# ------------------------------------------------------------------------------
get_image_tag() {
    case "$1" in
        ubuntu)     echo "pelcod-builder:ubuntu-24.04" ;;
        fedora)     echo "pelcod-builder:fedora-41" ;;
        windows)    echo "pelcod-builder:windows-cross" ;;
        freebsd)    echo "pelcod-builder:freebsd-14.2-cross" ;;
        freebsd-vm) echo "pelcod-builder:freebsd-14.2-vm" ;;
    esac
}

get_dockerfile_rel() {
    case "$1" in
        ubuntu)     echo "ubuntu/Dockerfile" ;;
        fedora)     echo "fedora/Dockerfile" ;;
        windows)    echo "windows/Dockerfile.cross" ;;
        freebsd)    echo "freebsd/Dockerfile.cross" ;;
        freebsd-vm) echo "freebsd/Dockerfile.vm" ;;
    esac
}

get_dist_subdir() {
    case "$1" in
        ubuntu)     echo "ubuntu" ;;
        fedora)     echo "fedora" ;;
        windows)    echo "windows" ;;
        freebsd|freebsd-vm) echo "freebsd" ;;
    esac
}

ensure_image_built() {
    local svc="$1"
    local img_tag
    img_tag="$(get_image_tag "${svc}")"
    local df_rel
    df_rel="$(get_dockerfile_rel "${svc}")"

    local need_build="false"
    if [ "${REBUILD_IMAGE}" = "true" ] || [ "${BUILD_IMAGE_ONLY}" = "true" ]; then
        need_build="true"
    elif ! "${CONTAINER_ENGINE}" image inspect "${img_tag}" >/dev/null 2>&1; then
        need_build="true"
    fi

    if [ "${need_build}" = "true" ]; then
        echo "----------------------------------------------------------------------"
        echo " [IMAGE] Building ${img_tag} (${df_rel}) ..."
        echo "----------------------------------------------------------------------"
        "${CONTAINER_ENGINE}" build \
            -t "${img_tag}" \
            -f "${DOCKER_DIR}/${df_rel}" \
            "${DOCKER_DIR}"
    fi
}

run_os_container() {
    local svc="$1"
    local img_tag
    img_tag="$(get_image_tag "${svc}")"
    local dist_sub
    dist_sub="$(get_dist_subdir "${svc}")"

    mkdir -p "${REPO_ROOT}/dist/${dist_sub}"

    local effective_run_tests="${RUN_TESTS}"
    if [ "${svc}" = "freebsd" ] && [ "${RUN_TESTS}" = "true" ]; then
        echo "[WARN] 'freebsd' uses Linux-hosted Clang/LLD sysroot cross-compilation."
        echo "       Skipping host CTest execution for 'freebsd' (use '--os freebsd-vm' for native FreeBSD kernel CTest)."
        effective_run_tests="false"
    elif [ "${svc}" = "freebsd-vm" ] && [ "${RUN_TESTS_EXPLICIT}" = "false" ]; then
        effective_run_tests="true"
    fi

    local windows_toolchain="/opt/windows-toolchain/toolchain-clang-cl-xwin.cmake"
    if [ "${svc}" = "windows" ] && [ "${COMPILER_FAMILY}" = "mingw" ]; then
        windows_toolchain="/opt/windows-toolchain/toolchain-mingw64.cmake"
    fi

    # Build common docker/podman run flags
    local run_args=(
        --rm
        -e "TARGET_OS=${dist_sub}"
        -e "BUILD_MODE=${BUILD_MODE}"
        -e "BUILD_CONFIG=${BUILD_CONFIG}"
        -e "COMPILER_FAMILY=${COMPILER_FAMILY}"
        -e "CMAKE_TARGET=${CMAKE_TARGET}"
        -e "RUN_TESTS=${effective_run_tests}"
        -e "CTEST_FILTER=${CTEST_FILTER}"
        -e "RUN_PACKAGE=${RUN_PACKAGE}"
        -e "CLEAN_BUILD=${CLEAN_BUILD}"
        -e "RECONFIGURE=${RECONFIGURE}"
        -e "WARNINGS_AS_ERRORS=${WARNINGS_AS_ERRORS}"
        -e "ENABLE_HARDENING=${ENABLE_HARDENING}"
        -e "ENABLE_ONVIF=${ENABLE_ONVIF}"
        -e "ENABLE_VIDEO=${ENABLE_VIDEO}"
        -e "ENABLE_DJI=${ENABLE_DJI}"
        -e "PARALLEL_JOBS=${PARALLEL_JOBS}"
        -e "HOST_UID=${HOST_UID}"
        -e "HOST_GID=${HOST_GID}"
        -v "${REPO_ROOT}:/workspace"
    )

    if [ "${svc}" != "freebsd-vm" ]; then
        run_args+=(
            -v "pelcod-ccache:/cache/ccache"
            -v "pelcod-build-${svc}:/build"
        )
    fi

    case "${svc}" in
        ubuntu)
            run_args+=(-e "CPACK_GENERATOR=${CPACK_GENERATOR:-DEB;TGZ}")
            ;;
        fedora)
            run_args+=(-e "CPACK_GENERATOR=${CPACK_GENERATOR:-RPM;TGZ}")
            ;;
        windows)
            run_args+=(
                -e "CMAKE_TOOLCHAIN_FILE=${windows_toolchain}"
                -e "QT_HOST_PATH=/usr"
                -e "CPACK_GENERATOR=${CPACK_GENERATOR:-ZIP}"
            )
            ;;
        freebsd)
            run_args+=(
                -e "CMAKE_TOOLCHAIN_FILE=/opt/freebsd-toolchain/toolchain-freebsd14.cmake"
                -e "QT_HOST_PATH=/usr"
                -e "CPACK_GENERATOR=${CPACK_GENERATOR:-TGZ}"
            )
            ;;
        freebsd-vm)
            run_args+=(
                -e "VM_MEMORY_MB=${VM_MEMORY_MB:-4096}"
                -e "CPACK_GENERATOR=${CPACK_GENERATOR:-TGZ}"
            )
            if [ -e /dev/kvm ]; then
                run_args+=(--device /dev/kvm:/dev/kvm)
            fi
            ;;
    esac

    if [ "${INTERACTIVE_SHELL}" = "true" ]; then
        echo "[SHELL] Launching interactive shell inside ${img_tag} ..."
        "${CONTAINER_ENGINE}" run -it "${run_args[@]}" "${img_tag}" /bin/bash
        return $?
    fi

    "${CONTAINER_ENGINE}" run "${run_args[@]}" "${img_tag}"
}

# ------------------------------------------------------------------------------
# Execute Build Loop Across Requested OS Targets
# ------------------------------------------------------------------------------
echo "======================================================================"
echo " PelcoD-Controller Multi-OS Container Orchestrator"
echo "----------------------------------------------------------------------"
echo " Container Engine   : ${CONTAINER_ENGINE}"
echo " Target OS List     : ${OS_TARGETS[*]}"
echo " Build Mode         : ${BUILD_MODE}"
echo " Build Config       : ${BUILD_CONFIG}"
echo " Compiler Family    : ${COMPILER_FAMILY}"
echo " CMake Target       : ${CMAKE_TARGET:-<all>}"
echo " Run Unit Tests     : ${RUN_TESTS}"
echo " Generate Packages  : ${RUN_PACKAGE}"
echo " Parallel Jobs      : ${PARALLEL_JOBS}"
echo "======================================================================"

declare -a SUMMARY_OS=()
declare -a SUMMARY_STATUS=()
declare -a SUMMARY_DURATION=()
OVERALL_RC=0

for os_target in "${OS_TARGETS[@]}"; do
    START_TS="$(date +%s)"
    echo ""
    echo ">>> [${os_target}] Starting container workflow ..."

    if ensure_image_built "${os_target}"; then
        if [ "${BUILD_IMAGE_ONLY}" = "true" ]; then
            END_TS="$(date +%s)"
            SUMMARY_OS+=("${os_target}")
            SUMMARY_STATUS+=("IMAGE_BUILT")
            SUMMARY_DURATION+=("$((END_TS - START_TS))s")
            continue
        fi

        if run_os_container "${os_target}"; then
            END_TS="$(date +%s)"
            SUMMARY_OS+=("${os_target}")
            SUMMARY_STATUS+=("PASS")
            SUMMARY_DURATION+=("$((END_TS - START_TS))s")
        else
            END_TS="$(date +%s)"
            SUMMARY_OS+=("${os_target}")
            SUMMARY_STATUS+=("FAIL")
            SUMMARY_DURATION+=("$((END_TS - START_TS))s")
            OVERALL_RC=1
        fi
    else
        END_TS="$(date +%s)"
        SUMMARY_OS+=("${os_target}")
        SUMMARY_STATUS+=("IMAGE_FAIL")
        SUMMARY_DURATION+=("$((END_TS - START_TS))s")
        OVERALL_RC=1
    fi
done

# ------------------------------------------------------------------------------
# Print Consolidated Summary Table
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo " Multi-OS Container Build Summary"
echo "----------------------------------------------------------------------"
printf " %-14s | %-12s | %-10s | %s\n" "TARGET OS" "STATUS" "DURATION" "ARTIFACT DIRECTORY"
echo "----------------+--------------+------------+-------------------------"
for idx in "${!SUMMARY_OS[@]}"; do
    os_name="${SUMMARY_OS[$idx]}"
    status_str="${SUMMARY_STATUS[$idx]}"
    dur_str="${SUMMARY_DURATION[$idx]}"
    dist_dir="dist/$(get_dist_subdir "${os_name}")/"
    printf " %-14s | %-12s | %-10s | %s\n" "${os_name}" "${status_str}" "${dur_str}" "${dist_dir}"
done
echo "======================================================================"

exit "${OVERALL_RC}"

