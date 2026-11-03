cmake_minimum_required(VERSION 3.21)
project(MedCipher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(SEAL_BUILD_DEPS OFF CACHE BOOL "Do not build optional SEAL dependencies" FORCE)
set(SEAL_USE_MSGSL OFF CACHE BOOL "Disable Microsoft GSL" FORCE)

set(SEAL_USE_ZLIB OFF CACHE BOOL "Disable zlib compression" FORCE)
set(SEAL_USE_ZSTD OFF CACHE BOOL "Disable zstd compression" FORCE)
set(SEAL_USE_INTEL_HEXL OFF CACHE BOOL "Disable Intel HEXL" FORCE)
FetchContent_Declare(SEAL
    GIT_REPOSITORY https://github.com/microsoft/SEAL.git
    GIT_TAG v4.1.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SEAL)

add_executable(MedCipher src/main.cpp src/crypto/ckks_smoke.cpp)
target_link_libraries(MedCipher PRIVATE SEAL::seal)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME ckks_addition COMMAND MedCipher)
endif()
