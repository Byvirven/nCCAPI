# nCCAPI - Normalized Crypto-Chassis API Wrapper

A standardized, header-only C++ wrapper for the [Crypto-Chassis CCAPI](https://github.com/crypto-chassis/ccapi) library. nCCAPI provides a unified interface similar to CCXT, allowing developers to interact with multiple cryptocurrency exchanges using a single, consistent API.

## Documentation

*   **[Architecture Overview](docs/ARCHITECTURE.md)**: Learn about the Unified Session, Pimpl idiom, and compilation optimizations.
*   **[Exchange Details & Quirks](docs/EXCHANGE_DETAILS.md)**: Detailed status of each exchange, including specific implementation notes and workarounds.

## Features

*   **Unified Interface**: Access multiple exchanges (Binance, Coinbase, Kraken, OKX, etc.) through a single `nccapi::Client`.
*   **Simplified Data Structures**:
    *   Standardized `Instrument` struct for trading pairs, supporting Spot, Futures, Options, and Swaps.
    *   Standardized `Candle` struct for OHLCV data.
*   **Header-Only Wrapper**: Easy integration (though dependencies must be linked).
*   **Optimized Compilation**: Uses Pimpl and a **Unified Session** architecture to keep build times extremely low during development.

## Supported Exchanges

**Note:** "Gap Risk" indicates exchanges that may return sparse historical data (omitting candles for periods with no trading volume), leading to potential time gaps in the requested range.

| Exchange | Status | Gap Risk (Sparse Data) |
| :--- | :--- | :--- |
| AscendEX | ✅ | ⚠️ Yes |
| Binance | ✅ | |
| Binance US | ✅ | |
| Binance Futures | ✅ | |
| Bitfinex | ✅ | ⚠️ Yes |
| Bitget | ✅ | |
| Bitget Futures | ✅ | |
| Bitmart | ✅ | ⚠️ Yes |
| BitMEX | ✅ | |
| Bitstamp | ✅ | ⚠️ Yes |
| Bybit | ✅ | |
| Coinbase | ✅ | ⚠️ Yes |
| Crypto.com | ✅ | ⚠️ Yes |
| Deribit | ✅ | ⚠️ Yes |
| Gate.io | ✅ | ⚠️ Yes |
| Gate.io Perpetual | ✅ | ⚠️ Yes |
| Gemini | ✅ | ⚠️ Yes |
| Huobi | ✅ | |
| Kraken | ✅ | ⚠️ Yes |
| Kraken Futures | ✅ | |
| KuCoin | ✅ | |
| KuCoin Futures | ✅ | |
| MEXC | ✅ | ⚠️ Yes |
| MEXC Futures | ✅ | ⚠️ Yes |
| OKX | ✅ | |
| WhiteBIT | ✅ | ⚠️ Yes |

## Dependencies & Installation

This project uses `git` submodules. You **must** clone recursively.

```bash
git clone --recursive <REPO_URL>
```

If you already cloned without recursive:
```bash
git submodule update --init --recursive
```

### System Requirements
*   **C++17** compiler.
*   **CMake 3.24+** (Required for dependency downloading behavior).
*   **OpenSSL** (System installed, e.g., `libssl-dev` on Debian/Ubuntu).
*   **Boost** (System installed or automatically downloaded via CMake).
*   **RapidJSON** (Automatically downloaded via CMake).

## Compilation

**CRITICAL WARNING:**
Do **NOT** use parallel compilation (`make -j` or `make -j4`). The CCAPI library heavily utilizes C++ templates, which requires significant RAM during compilation. Using parallel jobs will saturate your memory and crash the environment.

**Always compile sequentially:**

```bash
mkdir build
cd build
cmake ..
make
```

## Usage Example

```cpp
#include "nccapi/client.hpp"
#include <iostream>

int main() {
    try {
        // 1. Instantiate the Client (Factory)
        // This creates a single underlying CCAPI session shared by all exchanges.
        nccapi::Client client;

        // 2. Select an exchange
        std::string exchange_name = "coinbase";

        // 3. Fetch Instruments (Pairs)
        std::cout << "Fetching instruments for " << exchange_name << "..." << std::endl;
        auto instruments = client.get_pairs(exchange_name);

        // 4. Display results
        std::cout << "Found " << instruments.size() << " instruments." << std::endl;
        for (const auto& inst : instruments) {
            std::cout << "- " << inst.symbol << " (Base: " << inst.base
                      << ", Quote: " << inst.quote
                      << ", Type: " << inst.type << ")" << std::endl;
        }

        // 5. Fetch Historical Data (Candles)
        if (!instruments.empty()) {
            std::string symbol = instruments[0].id;
            std::cout << "Fetching candles for " << symbol << "..." << std::endl;

            // Fetch candles: 1 minute timeframe, defaults to recent lookback
            // Parameters: exchange, symbol, timeframe, start_time (optional), end_time (optional)
            auto candles = client.get_historical_candles(exchange_name, symbol, "1m");

            std::cout << "Received " << candles.size() << " candles." << std::endl;
            if (!candles.empty()) {
                std::cout << "First candle: " << candles[0].toString() << std::endl;
                std::cout << "Last candle: " << candles.back().toString() << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### API Reference

#### `get_historical_candles`

Fetches historical OHLCV (Open, High, Low, Close, Volume) data.

```cpp
std::vector<Candle> get_historical_candles(
    std::string exchange_name,
    std::string instrument_name,
    std::string timeframe,       // Default: "1m"
    std::string from_date = "",  // Default: Exchange specific minimum or recent
    std::string to_date = ""     // Default: Now
);
```
