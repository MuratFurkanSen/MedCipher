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

add_executable(MedCipher src/main.cpp src/crypto/ckks_smoke.cpp src/crypto/CKKSContext.cpp src/data/CSVReader.cpp src/scenarios/SumMean.cpp src/benchmark/Timing.cpp src/benchmark/Pipeline.cpp src/benchmark/Benchmark.cpp src/scenarios/Variance.cpp src/scenarios/VarianceReference.cpp src/scenarios/Relationships.cpp src/scenarios/RelationshipReference.cpp src/scenarios/WeightedScoring.cpp src/scenarios/ScoringReference.cpp src/scenarios/Polynomial.cpp src/scenarios/PolynomialReference.cpp)
target_link_libraries(MedCipher PRIVATE SEAL::seal)
target_compile_definitions(MedCipher PRIVATE
    MEDCIPHER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    MEDCIPHER_RESULTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/results"
    MEDCIPHER_BUILD_CONFIG="$<CONFIG>"
    MEDCIPHER_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

include(CTest)
if(BUILD_TESTING)
    add_executable(verification tests/verification.cpp src/benchmark/Timing.cpp src/data/CSVReader.cpp)
    target_include_directories(verification PRIVATE src)
    target_compile_definitions(verification PRIVATE MEDCIPHER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME reference_and_statistics COMMAND verification)
    add_test(NAME ckks_addition COMMAND MedCipher --smoke)
    add_test(NAME ckks_sum_mean COMMAND MedCipher)
    add_test(NAME ckks_relationships COMMAND MedCipher --relationships)
    add_test(NAME ckks_scoring COMMAND MedCipher --scoring-check)
    add_test(NAME ckks_polynomial COMMAND MedCipher --polynomial-check)
    add_test(NAME ckks_variance COMMAND MedCipher --variance-check)
endif()
