cmake_minimum_required(VERSION 3.10)
project(unified_ccapi_wrapper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Includes
include_directories(external/ccapi/include)
include_directories(external/include) # For rapidjson
include_directories(external/include/boost) # For boost
include_directories(src)

# Dependencies (OpenSSL, ZLib)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Compiler Flags
add_compile_options(-Wall -Wextra -pthread)

# Test Executable (REST Wrapper Test)
add_executable(test_wrapper src/main.cpp)
target_link_libraries(test_wrapper PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread)

# Global Test Executable (Scenario)
add_executable(test_global src/test_global.cpp)
target_link_libraries(test_global PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread)
