# Unified Crypto Wrapper

A standardized C++ wrapper for the [CCAPI](https://github.com/crypto-chassis/ccapi) library, inspired by the simplicity of CCXT. This wrapper provides a uniform interface for public and private APIs across different exchanges, handling specific quirks internally (e.g., Binance vs Coinbase).

## Features

- **Unified Interface**: Same method calls for all exchanges (`fetchTicker`, `fetchOrderBook`, `createOrder`, etc.).
- **Header-Only**: Easy integration (uses CCAPI which is header-only).
- **Public API Normalization**:
  - `fetchTicker`: Handles exchanges with missing `GET_BBOS` (e.g., Coinbase) via generic requests.
  - `fetchOrderBook`: Handles parameter differences and bugs (e.g., BinanceUS `limit` param, Coinbase endpoint).
  - `fetchOHLCV`: Standardized candlestick retrieval.
- **Private API Support**:
  - `createOrder`: Unified parameters for placing orders.
  - `fetchBalance`: Unified balance retrieval.
- **Quirk Handling**:
  - Automatically uses `GENERIC_PUBLIC_REQUEST` when native CCAPI implementation is missing or buggy for specific operations on specific exchanges.
  - Handles response correlation and synchronous waiting robustly.

## Dependencies

- **C++17** compatible compiler.
- **OpenSSL**: Required by CCAPI.
- **Boost** (headers only): `system`, `asio`, `beast`, etc.
- **RapidJSON** (headers only): Included in `external/include` if not present.
- **CCAPI**: Included in `external/ccapi`.

## Build Instructions

1. **Clone the repository** (with submodules if any, or ensure `external/` is populated):
   ```bash
   git clone <repo_url>
   cd <repo_directory>
   ```

2. **Build with CMake**:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Run Test**:
   ```bash
   ./test_wrapper
   ```

## Usage Example

### Public API

```cpp
#include "UnifiedExchange.hpp"
#include <iostream>

using namespace unified_crypto;

int main() {
    try {
        // Initialize Exchange (e.g., "binance-us", "coinbase")
        UnifiedExchange exchange("binance-us");

        // Fetch Ticker
        Ticker ticker = exchange.fetchTicker("BTCUSDT");
        std::cout << "Ticker: Bid=" << ticker.bidPrice << " Ask=" << ticker.askPrice << std::endl;

        // Fetch Order Book
        OrderBook book = exchange.fetchOrderBook("BTCUSDT", 5);
        std::cout << "Top Bid: " << book.bids[0].price << std::endl;

        // Fetch OHLCV
        auto candles = exchange.fetchOHLCV("BTCUSDT", "60", 5);
        if(!candles.empty()) {
            std::cout << "Last Close: " << candles[0].close << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
```

### Private API

To use private methods, provide an `ExchangeConfig` with API keys.

```cpp
ExchangeConfig config;
config.apiKey = "YOUR_API_KEY";
config.apiSecret = "YOUR_API_SECRET";
// config.passphrase = "YOUR_PASSPHRASE"; // For Coinbase, OKX, etc.

UnifiedExchange exchange("binance-us", config);

// Check Balance
auto balances = exchange.fetchBalance();
std::cout << "USDT Balance: " << balances["USDT"] << std::endl;

// Place Limit Order
std::string orderId = exchange.createOrder("BTCUSDT", "BUY", 0.001, 50000.0);
std::cout << "Order Placed: " << orderId << std::endl;
```

## Supported Exchanges (Verified)

- **BinanceUS** (`binance-us`): Full support (Ticker, OrderBook, OHLCV, Private).
- **Coinbase** (`coinbase`): Full support (Ticker, OrderBook via generic fallback).
- **Binance** (`binance`): Supported but requires non-US IP (Geo-blocked in some environments).
- *Others*: The wrapper is built on CCAPI, so other exchanges supported by CCAPI (Kraken, OKX, etc.) should work for standard operations, provided `GET_BBOS` and `GET_MARKET_DEPTH` are supported natively. If not, the `GENERIC_PUBLIC_REQUEST` pattern demonstrated for Coinbase can be extended.

## Streaming (Future Work)

Currently, the wrapper focuses on a robust REST-like interface (`fetch...`). CCAPI natively supports streaming. A future update could expose `subscribeTicker` and `subscribeOrderBook` using a similar unified callback mechanism, handling quirks like "quiet" markets (no data pushed until trade) by managing local state.
