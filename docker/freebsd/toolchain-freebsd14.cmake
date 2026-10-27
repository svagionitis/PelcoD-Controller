# ==============================================================================
# PelcoD-Controller - Linux-to-FreeBSD 14.2 amd64 Cross-Compilation Toolchain
# Compiler : clang-18 / clang++-18 + ld.lld-18
# Target   : x86_64-unknown-freebsd14.2
# Sysroot  : /opt/freebsd-sysroot (FreeBSD 14.2-RELEASE base.txz + pkg packages)
# ==============================================================================
set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_VERSION 14.2)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(FREEBSD_SYSROOT "/opt/freebsd-sysroot")
set(CMAKE_SYSROOT "${FREEBSD_SYSROOT}")

set(FREEBSD_TARGET_TRIPLE "x86_64-unknown-freebsd14.2")

set(CMAKE_C_COMPILER "clang-18")
set(CMAKE_CXX_COMPILER "clang++-18")
set(CMAKE_AR "llvm-ar-18")
set(CMAKE_RANLIB "llvm-ranlib-18")
set(CMAKE_C_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")

set(FREEBSD_COMMON_FLAGS
    "-isystem ${FREEBSD_SYSROOT}/usr/local/include -isystem ${FREEBSD_SYSROOT}/usr/local/include/qt6 -Wno-unused-command-line-argument"
)
set(FREEBSD_LINKER_FLAGS
    "-fuse-ld=lld-18 -L${FREEBSD_SYSROOT}/lib -L${FREEBSD_SYSROOT}/usr/lib -L${FREEBSD_SYSROOT}/usr/local/lib -L${FREEBSD_SYSROOT}/usr/local/lib/qt6 -Wl,-rpath-link,${FREEBSD_SYSROOT}/lib:${FREEBSD_SYSROOT}/usr/lib:${FREEBSD_SYSROOT}/usr/local/lib:${FREEBSD_SYSROOT}/usr/local/lib/qt6"
)

set(CMAKE_C_FLAGS_INIT "${FREEBSD_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${FREEBSD_COMMON_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${FREEBSD_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${FREEBSD_LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${FREEBSD_LINKER_FLAGS}")

# Configure pkg-config to resolve solely inside the FreeBSD sysroot
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${FREEBSD_SYSROOT}/usr/local/libdata/pkgconfig:${FREEBSD_SYSROOT}/usr/libdata/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "${FREEBSD_SYSROOT}/usr/local/libdata/pkgconfig:${FREEBSD_SYSROOT}/usr/libdata/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${FREEBSD_SYSROOT}")

# Use Linux host Qt6 code generators (moc, rcc, uic) while linking FreeBSD target Qt6 libraries
set(QT_HOST_PATH "/usr" CACHE PATH "Linux host Qt6 tools for FreeBSD cross-compilation")
set(QT_HOST_PATH_CMAKE_DIR "/usr/lib/x86_64-linux-gnu/cmake" CACHE PATH "Linux host Qt6 CMake directory")
set(QT_FORCE_FIND_TOOLS ON CACHE BOOL "Use Linux host Qt tools when cross-compiling for FreeBSD")

set(CMAKE_FIND_ROOT_PATH
    "${FREEBSD_SYSROOT}"
    "${FREEBSD_SYSROOT}/usr"
    "${FREEBSD_SYSROOT}/usr/local"
    "${FREEBSD_SYSROOT}/usr/local/lib/qt6"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

