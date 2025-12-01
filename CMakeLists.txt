cmake_minimum_required(VERSION 3.10)
project(unified_ccapi_wrapper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
# find_package(Boost REQUIRED COMPONENTS system thread date_time)
# RapidJSON is usually header-only in standard path, or use find_package/path if needed.
# On Ubuntu, rapidjson-dev installs to /usr/include/rapidjson.

# Includes
include_directories(external/ccapi/include)
include_directories(src)
include_directories(external/include)
include_directories(external/include/rapidjson/include)
include_directories(external/include/boost)
# include_directories(${Boost_INCLUDE_DIRS})
# /usr/include is default, so rapidjson should be found.

# Compiler Flags
# -w suppresses all warnings (requested by user to match CCAPI examples behavior)
add_compile_options(-w -pthread)

# Test Executable (REST Wrapper Test)
add_executable(test_wrapper src/main.cpp)
target_link_libraries(test_wrapper PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread ${Boost_LIBRARIES})

# Global Test Executable (Scenario)
add_executable(test_global src/test_global.cpp)
target_link_libraries(test_global PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread ${Boost_LIBRARIES})

# Geoblock Test Executable
add_executable(test_geoblock src/test_geoblock.cpp)
target_link_libraries(test_geoblock PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread ${Boost_LIBRARIES})

# Specific Exchange Tests
add_executable(test_binance_us src/tests/test_binance_us.cpp)
target_link_libraries(test_binance_us PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB dl pthread ${Boost_LIBRARIES})
