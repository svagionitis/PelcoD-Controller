#!/usr/bin/env bash
# ==============================================================================
# PelcoD-Controller - FreeBSD 14.2 QEMU/KVM VM Container Runner
#
# Boots a pre-provisioned FreeBSD 14.2-RELEASE amd64 QCOW2 guest inside Docker
# (using /dev/kvm hardware virtualization when available, or multi-threaded TCG),
# syncs /workspace into the FreeBSD VM over SSH/rsync, runs native FreeBSD
# CMake + Ninja + CTest + CPack, and syncs dist/freebsd/ back to the host.
# ==============================================================================
set -euo pipefail

if [ $# -gt 0 ]; then
    case "$1" in
        sh|bash|/bin/sh|/bin/bash)
            exec "$@"
            ;;
    esac
fi

TARGET_OS="${TARGET_OS:-freebsd}"
BUILD_MODE="${BUILD_MODE:-apps}"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
COMPILER_FAMILY="${COMPILER_FAMILY:-clang}"
CMAKE_TARGET="${CMAKE_TARGET:-}"
RUN_TESTS="${RUN_TESTS:-true}"
CTEST_FILTER="${CTEST_FILTER:-}"
RUN_PACKAGE="${RUN_PACKAGE:-true}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
WARNINGS_AS_ERRORS="${WARNINGS_AS_ERRORS:-ON}"
ENABLE_HARDENING="${ENABLE_HARDENING:-ON}"
VM_MEMORY_MB="${VM_MEMORY_MB:-4096}"
VM_CPUS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}"
SSH_PORT=2222

WORKSPACE_DIR="${WORKSPACE_DIR:-/workspace}"
VM_IMAGE_BASE="/opt/freebsd-vm/freebsd-14.2-base.qcow2"
VM_IMAGE_OVERLAY="/tmp/freebsd-14.2-run.qcow2"
VM_SSH_KEY="/opt/freebsd-vm/id_ed25519"

echo "======================================================================"
echo " PelcoD-Controller FreeBSD 14.2 QEMU/KVM VM Runner"
echo "----------------------------------------------------------------------"
echo " Build Mode   : ${BUILD_MODE}"
echo " Build Config : ${BUILD_CONFIG}"
echo " vCPUs / RAM  : ${VM_CPUS} cores / ${VM_MEMORY_MB} MB"
echo " Run CTest    : ${RUN_TESTS}"
echo "======================================================================"

# Create ephemeral QCOW2 overlay backed by the pre-provisioned FreeBSD 14.2 base image
qemu-img create -f qcow2 -b "${VM_IMAGE_BASE}" -F qcow2 "${VM_IMAGE_OVERLAY}" >/dev/null

# Select KVM hardware acceleration if /dev/kvm is accessible, otherwise multi-threaded TCG
QEMU_ACCEL=(-accel "tcg,thread=multi" -cpu max)
if [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    echo "[VM] /dev/kvm detected — enabling hardware KVM virtualization."
    QEMU_ACCEL=(-enable-kvm -cpu host)
else
    echo "[VM] /dev/kvm not available — using multi-threaded QEMU TCG emulation."
fi

qemu-system-x86_64 \
    "${QEMU_ACCEL[@]}" \
    -m "${VM_MEMORY_MB}" \
    -smp "${VM_CPUS}" \
    -drive "file=${VM_IMAGE_OVERLAY},if=virtio,format=qcow2" \
    -netdev "user,id=net0,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22" \
    -device virtio-net-pci,netdev=net0 \
    -display none \
    -serial mon:stdio \
    -daemonize \
    -pidfile /tmp/qemu-freebsd.pid

cleanup_vm() {
    if [ -f /tmp/qemu-freebsd.pid ]; then
        kill "$(cat /tmp/qemu-freebsd.pid)" 2>/dev/null || true
    fi
    rm -f "${VM_IMAGE_OVERLAY}" /tmp/qemu-freebsd.pid
}
trap cleanup_vm EXIT

SSH_OPTS=(
    -i "${VM_SSH_KEY}"
    -p "${SSH_PORT}"
    -o StrictHostKeyChecking=no
    -o UserKnownHostsFile=/dev/null
    -o ConnectTimeout=5
    -o LogLevel=ERROR
)

echo "[VM] Waiting for FreeBSD 14.2 SSH daemon on 127.0.0.1:${SSH_PORT} ..."
for _ in $(seq 1 90); do
    if ssh "${SSH_OPTS[@]}" root@127.0.0.1 "uname -a" >/dev/null 2>&1; then
        break
    fi
    sleep 2
done

ssh "${SSH_OPTS[@]}" root@127.0.0.1 "uname -a"

echo "[VM] Syncing /workspace into FreeBSD guest (/workspace) ..."
rsync -az --delete \
    --exclude='.git' \
    --exclude='build*' \
    --exclude='dist' \
    -e "ssh ${SSH_OPTS[*]}" \
    "${WORKSPACE_DIR}/" root@127.0.0.1:/workspace/

echo "[VM] Running native FreeBSD build & CTest inside guest ..."
ssh "${SSH_OPTS[@]}" root@127.0.0.1 \
    "TARGET_OS='${TARGET_OS}' \
     BUILD_MODE='${BUILD_MODE}' \
     BUILD_CONFIG='${BUILD_CONFIG}' \
     COMPILER_FAMILY='${COMPILER_FAMILY}' \
     CMAKE_TARGET='${CMAKE_TARGET}' \
     RUN_TESTS='${RUN_TESTS}' \
     CTEST_FILTER='${CTEST_FILTER}' \
     RUN_PACKAGE='${RUN_PACKAGE}' \
     CLEAN_BUILD='${CLEAN_BUILD}' \
     WARNINGS_AS_ERRORS='${WARNINGS_AS_ERRORS}' \
     ENABLE_HARDENING='${ENABLE_HARDENING}' \
     PARALLEL_JOBS='${VM_CPUS}' \
     /usr/local/bin/entrypoint-build.sh"

if [ "${RUN_PACKAGE}" = "true" ] && [ -z "${CMAKE_TARGET}" ]; then
    echo "[VM] Copying FreeBSD build artifacts back to host (${WORKSPACE_DIR}/dist/freebsd) ..."
    mkdir -p "${WORKSPACE_DIR}/dist/freebsd"
    rsync -az \
        -e "ssh ${SSH_OPTS[*]}" \
        root@127.0.0.1:/workspace/dist/freebsd/ "${WORKSPACE_DIR}/dist/freebsd/"
fi

echo "[OK] FreeBSD 14.2 QEMU/KVM VM build & test run completed."

