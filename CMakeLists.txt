cmake_minimum_required(VERSION 3.14)
project(nccapi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w") # Suppress warnings from CCAPI

# Includes
include_directories(include)
include_directories(external/ccapi/include)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

# External Projects for dependencies (if missing)
include(ExternalProject)

if(NOT BOOST_INCLUDE_DIR)
  ExternalProject_Add(
    boost
    URL https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/boost"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
endif()

if(NOT RAPIDJSON_INCLUDE_DIR)
  ExternalProject_Add(
    rapidjson
    URL https://github.com/Tencent/rapidjson/archive/3b2441b87f99ab65f37b141a7b548ebadb607b96.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/rapidjson"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
endif()

include_directories(${BOOST_INCLUDE_DIR})
include_directories(${RAPIDJSON_INCLUDE_DIR})

# Define CCAPI macros
add_definitions(-DCCAPI_ENABLE_EXCHANGE_ASCENDEX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_US)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITFINEX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITGET)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITGET_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITMART)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITMEX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITSTAMP)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BYBIT)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_COINBASE)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_CRYPTOCOM)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_DERIBIT)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_ERISX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GATEIO)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GEMINI)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_HUOBI)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KRAKEN)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KUCOIN)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_MEXC)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_MEXC_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_OKX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_WHITEBIT)

# Library Sources
set(SOURCES
    src/client.cpp
    src/exchanges/ascendex.cpp
    src/exchanges/binance.cpp
    src/exchanges/binance-coin-futures.cpp
    src/exchanges/binance-us.cpp
    src/exchanges/binance-usds-futures.cpp
    src/exchanges/bitfinex.cpp
    src/exchanges/bitget.cpp
    src/exchanges/bitget-futures.cpp
    src/exchanges/bitmart.cpp
    src/exchanges/bitmex.cpp
    src/exchanges/bitstamp.cpp
    src/exchanges/bybit.cpp
    src/exchanges/coinbase.cpp
    src/exchanges/cryptocom.cpp
    src/exchanges/deribit.cpp
    src/exchanges/erisx.cpp
    src/exchanges/gateio.cpp
    src/exchanges/gateio-perpetual-futures.cpp
    src/exchanges/gemini.cpp
    src/exchanges/huobi.cpp
    src/exchanges/huobi-coin-swap.cpp
    src/exchanges/huobi-usdt-swap.cpp
    src/exchanges/kraken.cpp
    src/exchanges/kraken-futures.cpp
    src/exchanges/kucoin.cpp
    src/exchanges/kucoin-futures.cpp
    src/exchanges/mexc.cpp
    src/exchanges/mexc-futures.cpp
    src/exchanges/okx.cpp
    src/exchanges/whitebit.cpp
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
