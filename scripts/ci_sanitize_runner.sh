#!/usr/bin/env bash
# ==============================================================================
# Pelco-D Controller - Local CI Sanitizer Runner
#
# Reproduces the exact GitHub Actions sanitizer workflows locally.
# Usage:
#   ./scripts/ci_sanitize_runner.sh [asan|tsan|ubsan|all]
# ==============================================================================

set -euo pipefail

MODE="${1:-all}"
WORKSPACE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

run_sanitizer() {
    local san_type="$1"
    local build_dir="${WORKSPACE_DIR}/build_${san_type}"
    local cmake_flag=""

    echo "========================================================================"
    echo "Running Sanitizer Suite: ${san_type^^}"
    echo "========================================================================"

    case "${san_type}" in
        asan)
            cmake_flag="-DENABLE_ASAN=ON"
            export ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1:halt_on_error=1"
            export LSAN_OPTIONS="suppressions=${WORKSPACE_DIR}/sanitizers/lsan.supp"
            ;;
        tsan)
            cmake_flag="-DENABLE_TSAN=ON"
            export TSAN_OPTIONS="halt_on_error=1:second_deadlock_stack=1:suppressions=${WORKSPACE_DIR}/sanitizers/tsan.supp"
            ;;
        ubsan)
            cmake_flag="-DENABLE_UBSAN=ON"
            export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:suppressions=${WORKSPACE_DIR}/sanitizers/ubsan.supp"
            ;;
        *)
            echo "Unknown sanitizer type: ${san_type}" >&2
            exit 1
            ;;
    esac

    cmake -B "${build_dir}" -G Ninja \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        "${cmake_flag}" \
        -DWARNINGS_AS_ERRORS=ON \
        -DPELCOD_ENABLE_ONVIF=ON \
        -DBUILD_TESTING=ON

    cmake --build "${build_dir}" --parallel "$(nproc)"

    ctest --test-dir "${build_dir}" --output-on-failure -j"$(nproc)" -L onvif
    echo "[OK] ${san_type^^} passed with 0 defects."
}

case "${MODE}" in
    asan|tsan|ubsan)
        run_sanitizer "${MODE}"
        ;;
    all)
        run_sanitizer "asan"
        run_sanitizer "tsan"
        run_sanitizer "ubsan"
        ;;
    *)
        echo "Usage: $0 [asan|tsan|ubsan|all]" >&2
        exit 1
        ;;
esac
