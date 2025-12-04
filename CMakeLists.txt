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
file(GLOB SESSION_SOURCES "src/sessions/*.cpp")
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

# Apply exchange-specific definitions to the specific SESSION source file ONLY
set_property(SOURCE src/sessions/kucoin-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)
set_property(SOURCE src/sessions/gateio_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_GATEIO)
set_property(SOURCE src/sessions/bitmart_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITMART)
set_property(SOURCE src/sessions/binance-coin-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
set_property(SOURCE src/sessions/huobi-usdt-swap_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP)
set_property(SOURCE src/sessions/erisx_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_ERISX)
set_property(SOURCE src/sessions/ascendex_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_ASCENDEX)
set_property(SOURCE src/sessions/bitmex_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITMEX)
set_property(SOURCE src/sessions/huobi-coin-swap_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
set_property(SOURCE src/sessions/bitfinex_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITFINEX)
set_property(SOURCE src/sessions/binance-us_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BINANCE_US)
set_property(SOURCE src/sessions/kraken-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES)
set_property(SOURCE src/sessions/coinbase_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_COINBASE)
set_property(SOURCE src/sessions/mexc_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_MEXC)
set_property(SOURCE src/sessions/whitebit_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_WHITEBIT)
set_property(SOURCE src/sessions/mexc-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES)
set_property(SOURCE src/sessions/kucoin_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_KUCOIN)
set_property(SOURCE src/sessions/bitget-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES)
set_property(SOURCE src/sessions/huobi_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_HUOBI)
set_property(SOURCE src/sessions/deribit_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_DERIBIT)
set_property(SOURCE src/sessions/gateio-perpetual-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
set_property(SOURCE src/sessions/kraken_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_KRAKEN)
set_property(SOURCE src/sessions/bybit_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BYBIT)
set_property(SOURCE src/sessions/okx_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_OKX)
set_property(SOURCE src/sessions/gemini_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_GEMINI)
set_property(SOURCE src/sessions/bitstamp_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITSTAMP)
set_property(SOURCE src/sessions/cryptocom_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_CRYPTOCOM)
set_property(SOURCE src/sessions/binance_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BINANCE)
set_property(SOURCE src/sessions/binance-usds-futures_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
set_property(SOURCE src/sessions/bitget_session.cpp PROPERTY COMPILE_DEFINITIONS CCAPI_ENABLE_EXCHANGE_BITGET)

# Test Executable
add_executable(test_structure tests/test_structure.cpp)
target_link_libraries(test_structure nccapi OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB dl)
