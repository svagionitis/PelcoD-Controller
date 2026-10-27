# ==============================================================================
# PelcoD-Controller - Linux-to-Windows x64 Cross-Compilation CMake Toolchain
# Compiler : clang-cl-18 + lld-link-18 (MSVC ABI)
# SDK/CRT  : Microsoft Visual C++ CRT & Windows SDK via xwin (/opt/xwin/splat)
# ==============================================================================
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(XWIN_DIR "/opt/xwin/splat")
set(TARGET_ARCH "x86_64")

set(CMAKE_C_COMPILER "clang-cl-18")
set(CMAKE_CXX_COMPILER "clang-cl-18")
set(CMAKE_LINKER "lld-link-18")
set(CMAKE_AR "llvm-lib-18")
set(CMAKE_RC_COMPILER "llvm-rc-18")
set(CMAKE_MT "llvm-mt-18")

set(CMAKE_C_COMPILER_TARGET "x86_64-pc-windows-msvc")
set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc")

set(XWIN_COMPILE_FLAGS
    "--target=x86_64-pc-windows-msvc -fms-compatibility -fms-extensions /EHsc -Wno-unused-command-line-argument /imsvc${XWIN_DIR}/crt/include /imsvc${XWIN_DIR}/sdk/include/ucrt /imsvc${XWIN_DIR}/sdk/include/um /imsvc${XWIN_DIR}/sdk/include/shared /imsvc/opt/vcpkg/installed/x64-windows/include"
)

set(XWIN_RC_FLAGS
    "/I${XWIN_DIR}/crt/include /I${XWIN_DIR}/sdk/include/ucrt /I${XWIN_DIR}/sdk/include/um /I${XWIN_DIR}/sdk/include/shared"
)

set(XWIN_LINK_FLAGS
    "/machine:x64 /libpath:${XWIN_DIR}/crt/lib/${TARGET_ARCH} /libpath:${XWIN_DIR}/sdk/lib/um/${TARGET_ARCH} /libpath:${XWIN_DIR}/sdk/lib/ucrt/${TARGET_ARCH} /libpath:/opt/vcpkg/installed/x64-windows/lib"
)

set(CMAKE_C_FLAGS_INIT "${XWIN_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${XWIN_COMPILE_FLAGS}")
set(CMAKE_RC_FLAGS_INIT "${XWIN_RC_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${XWIN_LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${XWIN_LINK_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${XWIN_LINK_FLAGS}")

# Configure pkg-config to resolve only Windows target .pc files in vcpkg
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "/opt/vcpkg/installed/x64-windows/lib/pkgconfig:/opt/vcpkg/installed/x64-windows/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "/opt/vcpkg/installed/x64-windows/lib/pkgconfig:/opt/vcpkg/installed/x64-windows/share/pkgconfig")

# Use Linux host Qt6 code generators (moc, rcc, uic) while linking Windows target Qt6 libraries
set(QT_HOST_PATH "/usr" CACHE PATH "Linux host Qt6 installation for moc/rcc/uic")
set(QT_HOST_PATH_CMAKE_DIR "/usr/lib/x86_64-linux-gnu/cmake" CACHE PATH "Linux host Qt6 CMake directory")
set(QT_FORCE_FIND_TOOLS ON CACHE BOOL "Use host Qt tools when cross-compiling")

set(CMAKE_FIND_ROOT_PATH
    "/opt/qt6-win/6.6.1/msvc2019_64"
    "/opt/vcpkg/installed/x64-windows"
    "${XWIN_DIR}/sdk"
    "${XWIN_DIR}/crt"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

