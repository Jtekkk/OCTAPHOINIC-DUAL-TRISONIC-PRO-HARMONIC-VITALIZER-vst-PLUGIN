# =============================================================================
#  toolchain-mingw-w64.cmake  --  Cross-compile Windows (x86-64) binaries on
#  Linux with the MinGW-w64 GCC toolchain.
#
#  Intended for the framework-independent DSP core + tests:
#    cmake -S . -B build-win-test \
#          -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake \
#          -DOCTA_BUILD_PLUGIN=OFF
#
#  NOTE: the JUCE plugin cannot be built with MinGW -- JUCE 8 rejects it
#  (#error "MinGW is not supported"). Build the plugin for Windows with MSVC
#  (CI or local Visual Studio). See docs/BUILD_WINDOWS.md.
# =============================================================================
set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Use the POSIX-threads variant: std::thread / std::mutex (used by JUCE) need it.
find_program(_octa_cc  ${TOOLCHAIN_PREFIX}-gcc-posix)
find_program(_octa_cxx ${TOOLCHAIN_PREFIX}-g++-posix)
if(NOT _octa_cc)
    set(_octa_cc  ${TOOLCHAIN_PREFIX}-gcc)
    set(_octa_cxx ${TOOLCHAIN_PREFIX}-g++)
endif()

set(CMAKE_C_COMPILER   ${_octa_cc})
set(CMAKE_CXX_COMPILER ${_octa_cxx})
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Search host paths for programs (juceaide), target paths for libs/headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Produce self-contained binaries (no MinGW runtime DLLs required at load time).
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static -static-libgcc -static-libstdc++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
