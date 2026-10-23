cmake_minimum_required(VERSION 3.15)

project(lzham
    VERSION 1.0.1
    DESCRIPTION "LZHAM - lossless data compression codec with LZMA-like ratios and fast decompression"
    LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# BUILD_X64 used to be the only way to select a 32-bit build. It is gone: use a toolchain file or
# pass -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 so that the choice is not tied to this project.
if(DEFINED BUILD_X64)
    message(DEPRECATION
        "BUILD_X64 is obsolete and ignored. For a 32-bit build pass "
        "-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 (GCC/Clang) or use a toolchain file.")
endif()

option(LZHAM_BUILD_SHARED "Build the shared library (lzham_x86.dll / lzham_x64.dll / liblzhamdll.so)" ON)
option(LZHAM_BUILD_STATIC "Build the static library (lzhamlib_x86.lib / liblzhamlib.a)" ON)
option(LZHAM_BUILD_TESTS "Build lzhamtest and the CTest smoke tests" ON)
option(LZHAM_ENABLE_MULTITHREADING "Enable multithreaded compression (Win32 threads or pthreads)" ON)
option(LZHAM_INSTALL "Generate install rules" ON)

if(NOT LZHAM_BUILD_SHARED AND NOT LZHAM_BUILD_STATIC)
    message(FATAL_ERROR "Enable at least one of LZHAM_BUILD_SHARED or LZHAM_BUILD_STATIC.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LZHAM_ARCH_SUFFIX "x64")
else()
    set(LZHAM_ARCH_SUFFIX "x86")
endif()

set(LZHAM_PUBLIC_HEADERS
    include/lzham.h
    include/lzham_dynamic_lib.h
    include/lzham_static_lib.h
    include/lzham_exports.inc
    include/zlib.h)

set(LZHAM_DECOMP_SOURCES
    lzhamdecomp/lzham_assert.cpp
    lzhamdecomp/lzham_checksum.cpp
    lzhamdecomp/lzham_huffman_codes.cpp
    lzhamdecomp/lzham_lzdecompbase.cpp
    lzhamdecomp/lzham_lzdecomp.cpp
    lzhamdecomp/lzham_mem.cpp
    lzhamdecomp/lzham_platform.cpp
    lzhamdecomp/lzham_prefix_coding.cpp
    lzhamdecomp/lzham_symbol_codec.cpp
    lzhamdecomp/lzham_timer.cpp
    lzhamdecomp/lzham_vector.cpp
    lzhamdecomp/lzham_core.h
    lzhamdecomp/lzham_decomp.h
    lzhamdecomp/lzham_math.h
    lzhamdecomp/lzham_platform.h
    lzhamdecomp/lzham_traits.h
    lzhamdecomp/lzham_types.h)

set(LZHAM_COMP_SOURCES
    lzhamcomp/lzham_lzbase.cpp
    lzhamcomp/lzham_lzcomp.cpp
    lzhamcomp/lzham_lzcomp_internal.cpp
    lzhamcomp/lzham_lzcomp_state.cpp
    lzhamcomp/lzham_match_accel.cpp
    lzhamcomp/lzham_comp.h
    lzhamcomp/lzham_lzbase.h
    lzhamcomp/lzham_lzcomp_internal.h
    lzhamcomp/lzham_match_accel.h
    lzhamcomp/lzham_threading.h)

if(LZHAM_ENABLE_MULTITHREADING)
    if(WIN32)
        list(APPEND LZHAM_COMP_SOURCES lzhamcomp/lzham_win32_threading.cpp)
    else()
        list(APPEND LZHAM_COMP_SOURCES lzhamcomp/lzham_pthreads_threading.cpp)
    endif()
endif()

# Shared per-target settings. -fno-strict-aliasing is mandatory: the codec type-puns in several places.
function(lzham_configure_target target)
    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET default
        CXX_VISIBILITY_PRESET default)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W3)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -fno-strict-aliasing)
    endif()

    if(UNIX)
        target_compile_definitions(${target} PRIVATE _LARGEFILE64_SOURCE=1 _FILE_OFFSET_BITS=64)
    endif()

    if(MINGW)
        # Ship self-contained DLLs/executables (no libgcc_s_*.dll / libstdc++-6.dll next to them).
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
    endif()

    if(NOT LZHAM_ENABLE_MULTITHREADING)
        target_compile_definitions(${target} PRIVATE LZHAM_ANSI_CPLUSPLUS=1)
    endif()
endfunction()

# --- Decompressor (internal, always static; it is embedded in the public libraries) ---
add_library(lzhamdecomp STATIC ${LZHAM_DECOMP_SOURCES})
lzham_configure_target(lzhamdecomp)
target_include_directories(lzhamdecomp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp)
set_target_properties(lzhamdecomp PROPERTIES EXPORT_NAME decomp)
add_library(lzham::decomp ALIAS lzhamdecomp)

# --- Compressor (internal, always static; it is embedded in the public libraries) ---
add_library(lzhamcomp STATIC ${LZHAM_COMP_SOURCES})
lzham_configure_target(lzhamcomp)
target_include_directories(lzhamcomp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamcomp)
target_link_libraries(lzhamcomp PRIVATE lzhamdecomp)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(lzhamcomp PRIVATE Threads::Threads)
    # The compressor's statistics code calls sqrt().
    target_link_libraries(lzhamcomp PRIVATE m)
endif()
set_target_properties(lzhamcomp PROPERTIES EXPORT_NAME comp)
add_library(lzham::comp ALIAS lzhamcomp)

# --- Public shared library ---
if(LZHAM_BUILD_SHARED)
    add_library(lzhamdll SHARED lzhamdll/lzham_api.cpp)
    if(WIN32)
        target_sources(lzhamdll PRIVATE lzhamdll/lzham_dll_main.cpp)
    endif()
    lzham_configure_target(lzhamdll)
    target_compile_definitions(lzhamdll PRIVATE LZHAM_EXPORTS)
    target_include_directories(lzhamdll
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp
            ${CMAKE_CURRENT_SOURCE_DIR}/lzhamcomp)
    target_link_libraries(lzhamdll PRIVATE lzhamcomp)
    set_target_properties(lzhamdll PROPERTIES
        OUTPUT_NAME lzhamdll
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
    if(WIN32)
        # Windows keeps the historical file names: lzham_x86.dll / lzham_x64.dll (and *D.dll in debug).
        # MinGW would otherwise produce liblzham_x64.dll, but the public header hardcodes lzham_x64.dll.
        set_target_properties(lzhamdll PROPERTIES
            PREFIX ""
            OUTPUT_NAME "lzham_${LZHAM_ARCH_SUFFIX}"
            DEBUG_POSTFIX "D")
    endif()
    set_target_properties(lzhamdll PROPERTIES EXPORT_NAME lzham)
    add_library(lzham::lzham ALIAS lzhamdll)
endif()

# --- Public static library ---
if(LZHAM_BUILD_STATIC)
    add_library(lzhamlib STATIC lzhamlib/lzham_lib.cpp)
    lzham_configure_target(lzhamlib)
    target_include_directories(lzhamlib
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp
            ${CMAKE_CURRENT_SOURCE_DIR}/lzhamcomp)
    target_link_libraries(lzhamlib PRIVATE lzhamcomp)
    set_target_properties(lzhamlib PROPERTIES OUTPUT_NAME lzhamlib)
    if(WIN32)
        # Windows keeps the historical file names: lzhamlib_x86.lib / lzhamlib_x64.lib (and *D.lib in debug).
        set_target_properties(lzhamlib PROPERTIES
            OUTPUT_NAME "lzhamlib_${LZHAM_ARCH_SUFFIX}"
            DEBUG_POSTFIX "D")
    endif()
    set_target_properties(lzhamlib PROPERTIES EXPORT_NAME lzham_static)
    add_library(lzham::lzham_static ALIAS lzhamlib)
endif()

# --- Command line test app ---
if(LZHAM_BUILD_TESTS)
    enable_testing()

    add_executable(lzhamtest lzhamtest/lzhamtest.cpp lzhamtest/timer.cpp)
    target_include_directories(lzhamtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamtest
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamcomp)
    if(NOT MSVC)
        target_compile_options(lzhamtest PRIVATE -Wall -Wextra)
    endif()
    if(MINGW)
        target_link_options(lzhamtest PRIVATE -static-libgcc -static-libstdc++)
    endif()
    set_target_properties(lzhamtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

    if(WIN32)
        # lzhamtest loads the DLL at runtime on Windows (see LZHAM_USE_LZHAM_DLL in lzhamtest.cpp),
        # so the DLL is a runtime dependency, not a link dependency.
        if(LZHAM_BUILD_SHARED)
            add_dependencies(lzhamtest lzhamdll)
        else()
            message(WARNING "LZHAM_BUILD_SHARED=OFF: lzhamtest loads the DLL at runtime, it will not run on Windows.")
        endif()
    else()
        # On POSIX the test app links the static API and needs no runtime library path.
        if(LZHAM_BUILD_STATIC)
            target_link_libraries(lzhamtest PRIVATE lzhamlib)
        else()
            target_link_libraries(lzhamtest PRIVATE lzhamdll)
        endif()
    endif()

    # Public C API smoke test: compress/decompress every mode and level, verify payload and Adler-32.
    add_executable(lzham_roundtrip_test tests/test_roundtrip.c)
    target_include_directories(lzham_roundtrip_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(MSVC)
        target_compile_options(lzham_roundtrip_test PRIVATE /W3)
    else()
        target_compile_options(lzham_roundtrip_test PRIVATE -Wall -Wextra)
    endif()
    if(MINGW)
        target_link_options(lzham_roundtrip_test PRIVATE -static-libgcc -static-libstdc++)
    endif()
    set_target_properties(lzham_roundtrip_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
    if(LZHAM_BUILD_SHARED)
        target_link_libraries(lzham_roundtrip_test PRIVATE lzhamdll)
    else()
        target_link_libraries(lzham_roundtrip_test PRIVATE lzhamlib)
    endif()

    add_test(NAME lzham_roundtrip COMMAND lzham_roundtrip_test)

    # A second copy of the library with the element limit lowered to 8 MB, so the 4 GB compressed-stream limit
    # (upstream issue #19) can be tested in seconds instead of allocating gigabytes.
    add_library(lzham_limits STATIC
        ${LZHAM_DECOMP_SOURCES}
        ${LZHAM_COMP_SOURCES}
        lzhamlib/lzham_lib.cpp)
    lzham_configure_target(lzham_limits)
    target_compile_definitions(lzham_limits PRIVATE LZHAM_MAX_VECTOR_ELEMENTS=0x800000)
    target_include_directories(lzham_limits PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamdecomp
        ${CMAKE_CURRENT_SOURCE_DIR}/lzhamcomp)
    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(lzham_limits PRIVATE Threads::Threads m)
    endif()

    add_executable(lzham_output_limit_test tests/test_output_limit.c)
    target_include_directories(lzham_output_limit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(MSVC)
        target_compile_options(lzham_output_limit_test PRIVATE /W3)
    else()
        target_compile_options(lzham_output_limit_test PRIVATE -Wall -Wextra)
    endif()
    if(MINGW)
        target_link_options(lzham_output_limit_test PRIVATE -static-libgcc -static-libstdc++)
    endif()
    target_link_libraries(lzham_output_limit_test PRIVATE lzham_limits)
    set_target_properties(lzham_output_limit_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

    add_test(NAME lzham_output_limit COMMAND lzham_output_limit_test)
endif()

# --- Install ---
if(LZHAM_INSTALL)
    set(LZHAM_INSTALL_TARGETS)
    if(LZHAM_BUILD_SHARED)
        list(APPEND LZHAM_INSTALL_TARGETS lzhamdll)
    endif()
    if(LZHAM_BUILD_STATIC)
        list(APPEND LZHAM_INSTALL_TARGETS lzhamlib)
    endif()
    list(APPEND LZHAM_INSTALL_TARGETS lzhamdecomp lzhamcomp)

    install(TARGETS ${LZHAM_INSTALL_TARGETS}
        EXPORT lzhamTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(FILES ${LZHAM_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT lzhamTargets
        NAMESPACE lzham::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lzham)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lzhamConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/lzhamConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lzham)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/lzhamConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/lzhamConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/lzhamConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lzham)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lzham.pc.in ${CMAKE_CURRENT_BINARY_DIR}/lzham.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lzham.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
