cmake_minimum_required(VERSION 3.22.1)
option(HCVAULT_BUILD_TESTS "Build gtest binaries" ON)
if(HCVAULT_BUILD_TESTS)
    project(hcvault_native LANGUAGES C CXX)
else()
    project(hcvault_native LANGUAGES C)
endif()

set(HCVAULT_SANITIZE "" CACHE STRING "Sanitizers, e.g. address;undefined or thread")
if(HCVAULT_SANITIZE)
    string(REPLACE ";" "," HCVAULT_SAN_LIST "${HCVAULT_SANITIZE}")
    add_compile_options(-fsanitize=${HCVAULT_SAN_LIST} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${HCVAULT_SAN_LIST})
endif()

set(VC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/veracrypt)
set(VC_COMMON ${VC_ROOT}/Common)
set(VC_CRYPTO ${VC_ROOT}/Crypto)

option(HCVAULT_FORCE_PORTABLE_CRYPTO "Disable x86 AES-NI/SSE paths (use if cpu.c fails without -maes)" OFF)

set(HCVAULT_COMPILE_DEFS
    HCVAULT_BUILD
    VC_DCS_DISABLE_ARGON2
    TC_UNIX
    TC_LINUX
    NO_OPTIMIZED_VERSIONS
    VOLFORMAT
    CRYPTOPP_DISABLE_ASM
    CRYPTOPP_DISABLE_SSE2
    CRYPTOPP_DISABLE_SSSE3
    CRYPTOPP_BOOL_X64=0
    CRYPTOPP_BOOL_X86=0
    CRYPTOPP_BOOL_X32=0
    CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE=0
    CRYPTOPP_BOOL_AESNI_INTRINSICS_AVAILABLE=0
)

set(HCVAULT_C_FLAGS
    -std=gnu11
    -fvisibility=hidden
    -fms-extensions
    -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/hcvault_preinclude.h
)

if(ANDROID)
    if(ANDROID_ABI STREQUAL "x86" OR ANDROID_ABI STREQUAL "x86_64")
        list(APPEND HCVAULT_C_FLAGS -msse2)
    endif()
    set(HCVAULT_PAGE_SIZE_LINK_FLAGS
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384"
    )
elseif(NOT HCVAULT_FORCE_PORTABLE_CRYPTO)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        list(APPEND HCVAULT_C_FLAGS -msse2 -maes -mpclmul)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
        list(APPEND HCVAULT_C_FLAGS -msse2)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR STREQUAL "")
        # 64-bit host with an empty processor string (some cross/WSL setups): assume x86_64.
        list(APPEND HCVAULT_C_FLAGS -msse2 -maes -mpclmul)
    endif()
endif()

set(VERACRYPT_COMMON_SRC
    ${VC_COMMON}/Crc.c
    ${VC_COMMON}/Endian.c
    ${VC_COMMON}/Pkcs5.c
    ${VC_COMMON}/Crypto.c
    ${VC_COMMON}/Xts.c
    ${VC_COMMON}/Volumes.c
)

set(VERACRYPT_CRYPTO_SRC
    ${VC_CRYPTO}/cpu.c
    ${VC_CRYPTO}/Aescrypt.c
    ${VC_CRYPTO}/Aeskey.c
    ${VC_CRYPTO}/Aestab.c
    ${VC_CRYPTO}/Sha2.c
    ${VC_CRYPTO}/Serpent.c
    ${VC_CRYPTO}/Twofish.c
    ${VC_CRYPTO}/Whirlpool.c
    ${VC_CRYPTO}/Camellia.c
    ${VC_CRYPTO}/kuznyechik.c
    ${VC_CRYPTO}/Streebog.c
    ${VC_CRYPTO}/blake2s.c
)

set(HCVAULT_VFS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs/vfs_fat.c
)

set(HCVAULT_WRAPPER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/encryption_thread_pool_minimal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/hcvault_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_keyfiles.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_format.c
)

add_library(veracrypt_volume STATIC
    ${VERACRYPT_COMMON_SRC}
    ${VERACRYPT_CRYPTO_SRC}
    ${HCVAULT_WRAPPER_SRC}
)

target_include_directories(veracrypt_volume PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${VC_ROOT}
    ${VC_COMMON}
    ${VC_CRYPTO}
)

target_compile_definitions(veracrypt_volume PRIVATE ${HCVAULT_COMPILE_DEFS})
target_compile_options(veracrypt_volume PRIVATE ${HCVAULT_C_FLAGS})
target_link_libraries(veracrypt_volume PUBLIC m)

add_library(hcvault_vfs STATIC ${HCVAULT_VFS_SRC})
target_include_directories(hcvault_vfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs
)
target_compile_options(hcvault_vfs PRIVATE ${HCVAULT_C_FLAGS})
target_link_libraries(hcvault_vfs PUBLIC veracrypt_volume)

option(HCVAULT_BUILD_FUZZ "Build libFuzzer header target (Clang only)" OFF)

if(NOT ANDROID)
    add_executable(volume_smoke tests/volume_smoke.c)
    target_link_libraries(volume_smoke PRIVATE veracrypt_volume)

    add_executable(hcvault-mkvol ${CMAKE_CURRENT_SOURCE_DIR}/tools/hcvault-mkvol.c)
    target_link_libraries(hcvault-mkvol PRIVATE veracrypt_volume)

    add_executable(hcvault-seed-png ${CMAKE_CURRENT_SOURCE_DIR}/tools/hcvault-seed-png.c)
    target_link_libraries(hcvault-seed-png PRIVATE hcvault_vfs)

    add_executable(hcvault-vfs-digest ${CMAKE_CURRENT_SOURCE_DIR}/tools/hcvault-vfs-digest.c)
    target_link_libraries(hcvault-vfs-digest PRIVATE hcvault_vfs)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND HCVAULT_BUILD_FUZZ)
        add_executable(header_fuzz tests/header_fuzz.c)
        target_link_libraries(header_fuzz PRIVATE veracrypt_volume)
        target_compile_options(header_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(header_fuzz PRIVATE -fsanitize=fuzzer)
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FUSE3 QUIET fuse3)
    endif()
    if(FUSE3_FOUND)
        set(HCVAULT_MOUNT_LIB_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/loop_setup.c
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/fsprobe.c
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/mountctl.c
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/fuse_volume.c
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/registry.c
            ${CMAKE_CURRENT_SOURCE_DIR}/mount/sockctl.c
        )
        add_library(hcvault_mount STATIC ${HCVAULT_MOUNT_LIB_SRC})
        target_include_directories(hcvault_mount PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/mount
        )
        target_compile_definitions(hcvault_mount PUBLIC FUSE_USE_VERSION=31)
        target_compile_options(hcvault_mount PRIVATE ${HCVAULT_C_FLAGS})
        target_link_libraries(hcvault_mount PUBLIC veracrypt_volume ${FUSE3_LIBRARIES} pthread)
        target_include_directories(hcvault_mount PUBLIC ${FUSE3_INCLUDE_DIRS})

        add_executable(hcvault-mountd ${CMAKE_CURRENT_SOURCE_DIR}/mount/mountd_main.c)
        target_compile_options(hcvault-mountd PRIVATE ${HCVAULT_C_FLAGS})
        target_link_libraries(hcvault-mountd PRIVATE hcvault_mount)

        add_executable(mount_loop_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/mount_loop_test.c)
        target_compile_options(mount_loop_test PRIVATE ${HCVAULT_C_FLAGS})
        target_link_libraries(mount_loop_test PRIVATE hcvault_mount)
    endif()
endif()

if(HCVAULT_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(hcvault_volume_tests
        tests/volume_test.cpp
        tests/interop_test.cpp
    )
    target_link_libraries(hcvault_volume_tests PRIVATE veracrypt_volume GTest::gtest_main)
    target_compile_features(hcvault_volume_tests PRIVATE cxx_std_17)
    include(GoogleTest)
    gtest_discover_tests(hcvault_volume_tests)

    add_executable(vfs_smoke tests/vfs_smoke.cpp)
    target_link_libraries(vfs_smoke PRIVATE hcvault_vfs GTest::gtest_main)
    target_compile_features(vfs_smoke PRIVATE cxx_std_17)
    gtest_discover_tests(vfs_smoke)
endif()

if(ANDROID)
    add_library(hcvault_jni SHARED jni/hcvault_jni.c)
    target_link_libraries(hcvault_jni PRIVATE veracrypt_volume hcvault_vfs log)
    target_include_directories(hcvault_jni PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_options(hcvault_jni PRIVATE ${HCVAULT_PAGE_SIZE_LINK_FLAGS})
endif()
