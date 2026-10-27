# ==============================================================================
# PelcoD-Controller - Linux-to-Windows x64 Cross-Compilation CMake Toolchain
# Compiler : x86_64-w64-mingw32-gcc / x86_64-w64-mingw32-g++ (POSIX threads)
# ==============================================================================
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(MINGW_TRIPLE "x86_64-w64-mingw32")

set(CMAKE_C_COMPILER "${MINGW_TRIPLE}-gcc-posix")
set(CMAKE_CXX_COMPILER "${MINGW_TRIPLE}-g++-posix")
set(CMAKE_RC_COMPILER "${MINGW_TRIPLE}-windres")

set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/wine64")

set(QT_HOST_PATH "/usr" CACHE PATH "Linux host Qt6 installation for moc/rcc/uic")
set(QT_HOST_PATH_CMAKE_DIR "/usr/lib/x86_64-linux-gnu/cmake" CACHE PATH "Linux host Qt6 CMake directory")

set(CMAKE_FIND_ROOT_PATH
    "/usr/${MINGW_TRIPLE}"
    "/opt/vcpkg/installed/x64-mingw-dynamic"
    "/opt/qt6-win/6.6.1/mingw_64"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

