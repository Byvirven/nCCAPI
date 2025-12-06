cmake_minimum_required(VERSION 3.14)
project(nccapi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

include_directories(include)
include_directories(external/ccapi/include)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

include(ExternalProject)
if(NOT BOOST_INCLUDE_DIR)
  ExternalProject_Add(boost URL https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/boost" CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "" TEST_COMMAND "")
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
endif()
if(NOT RAPIDJSON_INCLUDE_DIR)
  ExternalProject_Add(rapidjson URL https://github.com/Tencent/rapidjson/archive/3b2441b87f99ab65f37b141a7b548ebadb607b96.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/rapidjson" CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "" TEST_COMMAND "")
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
endif()
include_directories(${BOOST_INCLUDE_DIR})
include_directories(${RAPIDJSON_INCLUDE_DIR})

# Library Sources
file(GLOB EXCHANGE_SOURCES "src/exchanges/*.cpp")
# Unified Session (One file to compile them all!)
set(SESSION_SOURCES "src/sessions/unified_session.cpp")

set(SOURCES
    src/client.cpp
    ${EXCHANGE_SOURCES}
    ${SESSION_SOURCES}
)

add_library(nccapi ${SOURCES})

# Dependencies for library
if (TARGET boost)
    add_dependencies(nccapi boost)
endif()
if (TARGET rapidjson)
    add_dependencies(nccapi rapidjson)
endif()

# Test Executable
add_executable(test_structure tests/test_structure.cpp)
target_link_libraries(test_structure nccapi OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB dl)

# Test Historical
add_executable(test_historical tests/test_historical.cpp)
target_link_libraries(test_historical nccapi OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB dl)
