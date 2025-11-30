cmake_minimum_required(VERSION 3.14)
project(UnifiedCryptoWrapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
# OpenSSL is required by CCAPI
find_package(OpenSSL REQUIRED)
find_package(ZLIB) # Optional but good for some exchanges
find_package(Threads REQUIRED)

# Include directories
# CCAPI
include_directories(external/ccapi/include)
# Boost & RapidJSON
include_directories(external/include/boost)
include_directories(external/include)

# Enable necessary CCAPI services and exchanges
# We can define them globally here or per target.
# For now, we enable Market Data and Execution Management for all major exchanges.
add_definitions(-DCCAPI_ENABLE_SERVICE_MARKET_DATA)
add_definitions(-DCCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)

# Enable specific exchanges (can be more selective to speed up build)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_US)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_COINBASE)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KRAKEN)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GATEIO)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_KUCOIN)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GEMINI)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BITSTAMP)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BYBIT)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_OKX)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_HUOBI)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
add_definitions(-DCCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
add_definitions(-DCCAPI_ENABLE_LOG_TRACE) # Enable logs for debugging

# Main library (header-only wrapper) interface
add_library(unified_crypto INTERFACE)
target_include_directories(unified_crypto INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    external/ccapi/include
    external/include
)

target_link_libraries(unified_crypto INTERFACE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(ZLIB_FOUND)
    target_link_libraries(unified_crypto INTERFACE ZLIB::ZLIB)
endif()

# Test Executable
add_executable(test_wrapper src/main.cpp)
target_link_libraries(test_wrapper PRIVATE unified_crypto)
