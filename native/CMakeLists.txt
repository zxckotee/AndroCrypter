cmake_minimum_required(VERSION 3.22.1)
option(HCVAULT_BUILD_TESTS "Build gtest binaries" ON)
if(HCVAULT_BUILD_TESTS)
    project(hcvault_native LANGUAGES C CXX)
else()
    project(hcvault_native LANGUAGES C)
endif()

set(VC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/veracrypt)
set(VC_COMMON ${VC_ROOT}/Common)
set(VC_CRYPTO ${VC_ROOT}/Crypto)

set(HCVAULT_COMPILE_DEFS
    HCVAULT_BUILD
    VC_DCS_DISABLE_ARGON2
    TC_UNIX
    TC_LINUX
    NO_OPTIMIZED_VERSIONS
    CRYPTOPP_BOOL_X64=0
    CRYPTOPP_BOOL_X86=0
    CRYPTOPP_BOOL_X32=0
    CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE=0
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
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    list(APPEND HCVAULT_C_FLAGS -msse2)
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

set(HCVAULT_WRAPPER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/encryption_thread_pool_minimal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/hcvault_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/vc_api.c
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

if(NOT ANDROID)
    add_executable(volume_smoke tests/volume_smoke.c)
    target_link_libraries(volume_smoke PRIVATE veracrypt_volume)
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
    )
    target_link_libraries(hcvault_volume_tests PRIVATE veracrypt_volume GTest::gtest_main)
    target_compile_features(hcvault_volume_tests PRIVATE cxx_std_17)
    include(GoogleTest)
    gtest_discover_tests(hcvault_volume_tests)
endif()

if(ANDROID)
    add_library(hcvault_jni SHARED jni/hcvault_jni.c)
    target_link_libraries(hcvault_jni PRIVATE veracrypt_volume log)
    target_include_directories(hcvault_jni PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_options(hcvault_jni PRIVATE "-Wl,-z,max-page-size=16384")
endif()
