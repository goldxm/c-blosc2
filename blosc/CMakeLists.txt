# Blosc - Blocked Shuffling and Compression Library
#
# Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
# https://blosc.org
# License: BSD 3-Clause (see LICENSE.txt)
#
# See LICENSE.txt for details about copyright and rights to use.

# A simple way to detect that we are using CMAKE
add_definitions(-DUSING_CMAKE)

set(INTERNAL_LIBS ${PROJECT_SOURCE_DIR}/internal-complibs)

# Hide symbols by default unless they're specifically exported.
# This makes it easier to keep the set of exported symbols the
# same across all compilers/platforms.
set(CMAKE_C_VISIBILITY_PRESET hidden)

# includes
set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
if(LZ4_FOUND)
    set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${LZ4_INCLUDE_DIR})
else()
    set(LZ4_LOCAL_DIR ${INTERNAL_LIBS}/lz4-1.9.4)
    set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${LZ4_LOCAL_DIR})
endif()

if(NOT DEACTIVATE_ZLIB)
    if(ZLIB_NG_FOUND)
        set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${ZLIB_NG_INCLUDE_DIR})
    elseif(ZLIB_FOUND)
        set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIR})
    else()
        set(ZLIB_LOCAL_DIR ${INTERNAL_LIBS}/${ZLIB_NG_DIR})
        set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${ZLIB_LOCAL_DIR})
    endif()
endif()

if(NOT DEACTIVATE_ZSTD)
    if(ZSTD_FOUND)
        set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${ZSTD_INCLUDE_DIR})
    else()
        set(ZSTD_LOCAL_DIR ${INTERNAL_LIBS}/zstd-1.5.5)
        set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIRS} ${ZSTD_LOCAL_DIR}
                ${ZSTD_LOCAL_DIR}/common)
    endif()
endif()

include_directories(${BLOSC_INCLUDE_DIRS})

# library sources
set(SOURCES ${SOURCES} blosc2.c blosclz.c fastcopy.c fastcopy.h schunk.c frame.c stune.c stune.h
        context.h delta.c delta.h shuffle-generic.c bitshuffle-generic.c trunc-prec.c trunc-prec.h
        timestamp.c sframe.c directories.c blosc2-stdio.c
        b2nd.c b2nd_utils.c)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL arm64)
    if(COMPILER_SUPPORT_SSE2)
        message(STATUS "Adding run-time support for SSE2")
        set(SOURCES ${SOURCES} shuffle-sse2.c bitshuffle-sse2.c)
    endif()
    if(COMPILER_SUPPORT_AVX2)
        message(STATUS "Adding run-time support for AVX2")
        set(SOURCES ${SOURCES} shuffle-avx2.c bitshuffle-avx2.c)
    endif()
endif()
if(COMPILER_SUPPORT_NEON)
    message(STATUS "Adding run-time support for NEON")
    set(SOURCES ${SOURCES} shuffle-neon.c bitshuffle-neon.c)
endif()
if(COMPILER_SUPPORT_ALTIVEC)
    message(STATUS "Adding run-time support for ALTIVEC")
    set(SOURCES ${SOURCES} shuffle-altivec.c bitshuffle-altivec.c)
endif()
set(SOURCES ${SOURCES} shuffle.c)

set(version_string ${BLOSC2_VERSION_MAJOR}.${BLOSC2_VERSION_MINOR}.${BLOSC2_VERSION_PATCH})

set(CMAKE_THREAD_PREFER_PTHREAD TRUE) # pre 3.1
set(THREADS_PREFER_PTHREAD_FLAG TRUE) # CMake 3.1+
if(WIN32)
    # try to use the system library
    find_package(Threads)
    if(NOT Threads_FOUND)
        message(STATUS "using the internal pthread library for win32 systems.")
        set(SOURCES ${SOURCES} win32/pthread.c)
    else()
        if(CMAKE_VERSION VERSION_LESS 3.1)
            set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
        else()
            set(LIBS ${LIBS} Threads::Threads)
        endif()
    endif()
else()
    find_package(Threads REQUIRED)
    if(CMAKE_VERSION VERSION_LESS 3.1)
        set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
    else()
        set(LIBS ${LIBS} Threads::Threads)
    endif()
    set(LIBS ${LIBS} ${CMAKE_DL_LIBS})
endif()

if(LZ4_FOUND)
    set(LIBS ${LIBS} ${LZ4_LIBRARY})
else()
    file(GLOB LZ4_FILES ${LZ4_LOCAL_DIR}/*.c)
    set(SOURCES ${SOURCES} ${LZ4_FILES})
    source_group("LZ4" FILES ${LZ4_FILES})
endif()

if(NOT DEACTIVATE_ZLIB)
    if(ZLIB_NG_FOUND)
        set(LIBS ${LIBS} ${ZLIB_NG_LIBRARY})
    elseif(ZLIB_FOUND)
        set(LIBS ${LIBS} ${ZLIB_LIBRARY})
    else()
        set(ZLIB_LOCAL_DIR ${INTERNAL_LIBS}/${ZLIB_NG_DIR})
        file(GLOB ZLIB_FILES ${ZLIB_LOCAL_DIR}/*.c)
        set(SOURCES ${SOURCES} ${ZLIB_FILES})
        source_group("Zlib" FILES ${ZLIB_FILES})
    endif()
endif()

if(NOT DEACTIVATE_ZSTD)
    if(ZSTD_FOUND)
        set(LIBS ${LIBS} ${ZSTD_LIBRARY})
    else()
        # Enable assembly code only when not using MSVC *and* x86 is there
        if((NOT MSVC) AND COMPILER_SUPPORT_SSE2)   # if SSE2 is here, this is an x86 platform
            message(STATUS "Adding support for assembly sources in ZSTD")
            file(GLOB ZSTD_DECOMPRESS_FILES ${ZSTD_LOCAL_DIR}/decompress/*.S)
        else()
            message(STATUS "Disabling support for assembly sources in ZSTD")
            add_compile_definitions("ZSTD_DISABLE_ASM")
        endif()
        file(GLOB ZSTD_DECOMPRESS_FILES ${ZSTD_DECOMPRESS_FILES}
                ${ZSTD_LOCAL_DIR}/decompress/*.c)
        file(GLOB ZSTD_COMMON_FILES ${ZSTD_LOCAL_DIR}/common/*.c)
        file(GLOB ZSTD_COMPRESS_FILES ${ZSTD_LOCAL_DIR}/compress/*.c)
        file(GLOB ZSTD_DICT_FILES ${ZSTD_LOCAL_DIR}/dictBuilder/*.c)
        set(ZSTD_FILES ${ZSTD_COMMON_FILES} ${ZSTD_COMPRESS_FILES}
            ${ZSTD_DECOMPRESS_FILES} ${ZSTD_DICT_FILES})
        set(SOURCES ${SOURCES} ${ZSTD_FILES})
        source_group("Zstd" FILES ${ZSTD_FILES})
    endif()
endif()

if(HAVE_IPP)
    set(LIBS ${LIBS} "${IPP_LIBRARIES}")
endif()

if(UNIX AND NOT APPLE)
    set(LIBS ${LIBS} "rt")
    set(LIBS ${LIBS} "m")
    # set(LIBS ${LIBS} "profiler")
endif()


# targets
if(BUILD_SHARED)
    add_library(blosc2_shared SHARED ${SOURCES})
    set_target_properties(blosc2_shared PROPERTIES OUTPUT_NAME blosc2)
    if(MSVC OR MINGW)
        set_target_properties(blosc2_shared PROPERTIES PREFIX lib)
    endif()
    set_target_properties(blosc2_shared PROPERTIES
            VERSION ${version_string}
            SOVERSION 2  # Change this when an ABI change happens
            )
    set_property(
            TARGET blosc2_shared
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_SHARED_LIBRARY)
endif()

# Based on the target architecture and hardware features supported
# by the C compiler, set hardware architecture optimization flags
# for specific shuffle implementations.
if(COMPILER_SUPPORT_SSE2)
    if(MSVC)
        # MSVC targets SSE2 by default on 64-bit configurations, but not 32-bit configurations.
        if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
            set_source_files_properties(
                    shuffle-sse2.c bitshuffle-sse2.c blosclz.c fastcopy.c
                    PROPERTIES COMPILE_OPTIONS "/arch:SSE2")
            set_property(
                    SOURCE shuffle.c
                    APPEND PROPERTY COMPILE_OPTIONS "/arch:SSE2")
        endif()
    else()
        set_source_files_properties(
                shuffle-sse2.c bitshuffle-sse2.c blosclz.c fastcopy.c
                PROPERTIES COMPILE_OPTIONS -msse2)
        set_property(
                SOURCE shuffle.c
                APPEND PROPERTY COMPILE_OPTIONS -msse2)
        # Add SIMD flags for the bytedelta filter and Intel (it seems that ARM64 does not need these)
        set_source_files_properties(
                ${PROJECT_SOURCE_DIR}/plugins/filters/bytedelta/bytedelta.c
                PROPERTIES COMPILE_OPTIONS "-mssse3")
    endif()

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows SSE2 is supported even though that file is
    # compiled without SSE2 support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_SSE2_ENABLED)

endif()
if(COMPILER_SUPPORT_AVX2)
    if(MSVC)
        set_source_files_properties(
                shuffle-avx2.c bitshuffle-avx2.c
                PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
        set_property(
                SOURCE shuffle.c
                APPEND PROPERTY COMPILE_OPTIONS "/arch:AVX2")
    else()
        set_source_files_properties(
                shuffle-avx2.c bitshuffle-avx2.c
                PROPERTIES COMPILE_OPTIONS -mavx2)
        set_property(
                SOURCE shuffle.c
                APPEND PROPERTY COMPILE_OPTIONS -mavx2)
    endif()

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows AVX2 is supported even though that file is
    # compiled without AVX2 support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_AVX2_ENABLED)
endif()
if(COMPILER_SUPPORT_NEON)
    set_source_files_properties(
            shuffle-neon.c bitshuffle-neon.c
            PROPERTIES COMPILE_OPTIONS "-flax-vector-conversions")
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_OPTIONS "-flax-vector-conversions")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL armv7l)
        # Only armv7l needs special -mfpu=neon flag; aarch64 doesn't.
      set_source_files_properties(
            shuffle-neon.c bitshuffle-neon.c
            PROPERTIES COMPILE_OPTIONS "-mfpu=neon -flax-vector-conversions")
      set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_OPTIONS "-mfpu=neon -flax-vector-conversions")
    endif()
    # Define a symbol for the shuffle-dispatch implementation
    # so it knows NEON is supported even though that file is
    # compiled without NEON support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_NEON_ENABLED)
endif()
if(COMPILER_SUPPORT_ALTIVEC)
    set_source_files_properties(
            shuffle-altivec.c bitshuffle-altivec.c
            PROPERTIES COMPILE_OPTIONS -DNO_WARN_X86_INTRINSICS)
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_OPTIONS -DNO_WARN_X86_INTRINSICS)

    # Define a symbol for the shuffle-dispatch implementation
    # so it knows ALTIVEC is supported even though that file is
    # compiled without ALTIVEC support (for portability).
    set_property(
            SOURCE shuffle.c
            APPEND PROPERTY COMPILE_DEFINITIONS SHUFFLE_ALTIVEC_ENABLED)
endif()

# When the option has been selected to compile the test suite,
# compile an additional version of blosc2_static which exports
# some normally-hidden symbols (to facilitate unit testing).
if(BUILD_TESTS)
    add_library(blosc_testing STATIC ${SOURCES})
    set_target_properties(blosc_testing PROPERTIES OUTPUT_NAME blosc_testing)
    if(MSVC OR MINGW)
        set_target_properties(blosc_testing PROPERTIES PREFIX lib)
    endif()
    set_property(
            TARGET blosc_testing
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_SHARED_LIBRARY)
    set_property(
            TARGET blosc_testing
            APPEND PROPERTY COMPILE_DEFINITIONS BLOSC_TESTING)
endif()

if(BUILD_SHARED)
    target_link_libraries(blosc2_shared ${LIBS})
    target_include_directories(blosc2_shared PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

if(BUILD_TESTS)
    target_link_libraries(blosc_testing ${LIBS})
    target_include_directories(blosc_testing PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

if(BUILD_STATIC)
    add_library(blosc2_static STATIC ${SOURCES})
    set_target_properties(blosc2_static PROPERTIES OUTPUT_NAME blosc2)
    set_target_properties(blosc2_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC OR MINGW)
        set_target_properties(blosc2_static PROPERTIES PREFIX lib)
    endif()
    target_link_libraries(blosc2_static ${LIBS})
    target_include_directories(blosc2_static PUBLIC ${BLOSC_INCLUDE_DIRS})
endif()

# install
if(BLOSC_INSTALL)
    install(FILES ${PROJECT_SOURCE_DIR}/include/blosc2.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT DEV)
    install(FILES ${PROJECT_SOURCE_DIR}/include/b2nd.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT DEV)
    install(FILES
            ${PROJECT_SOURCE_DIR}/include/blosc2/blosc2-export.h
            ${PROJECT_SOURCE_DIR}/include/blosc2/blosc2-common.h
            ${PROJECT_SOURCE_DIR}/include/blosc2/blosc2-stdio.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/blosc2 COMPONENT DEV)
    if(BUILD_PLUGINS)
        install(FILES
                ${PROJECT_SOURCE_DIR}/include/blosc2/filters-registry.h
                ${PROJECT_SOURCE_DIR}/include/blosc2/codecs-registry.h
                ${PROJECT_SOURCE_DIR}/include/blosc2/tuners-registry.h
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/blosc2 COMPONENT DEV)
    endif()

    if(BUILD_SHARED)
        install(TARGETS blosc2_shared
                LIBRARY COMPONENT LIB
                ARCHIVE COMPONENT DEV
                RUNTIME COMPONENT LIB)
    endif()
    if(BUILD_STATIC)
        install(TARGETS blosc2_static COMPONENT DEV)
    endif()
endif()
