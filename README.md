# nCCAPI - Normalized Crypto-Chassis API Wrapper

A standardized, header-only C++ wrapper for the [Crypto-Chassis CCAPI](https://github.com/crypto-chassis/ccapi) library. nCCAPI provides a unified interface similar to CCXT, allowing developers to interact with multiple cryptocurrency exchanges using a single, consistent API.

## Features

*   **Unified Interface**: Access multiple exchanges (Binance, Coinbase, Kraken, OKX, etc.) through a single `nccapi::Client`.
*   **Simplified Data Structures**: Standardized `Instrument` struct for trading pairs, supporting Spot, Futures, Options, and Swaps.
*   **Header-Only Wrapper**: Easy integration (though dependencies must be linked).
*   **Optimized Compilation**: Uses Pimpl and a **Unified Session** architecture to keep build times extremely low during development.

## Build Time Optimization

This project employs a "Unified Session" strategy.
*   **Problem**: CCAPI is header-only and uses heavy template meta-programming. Compiling support for 30+ exchanges usually takes 45+ minutes.
*   **Solution**: We isolate the CCAPI session instantiation into a single translation unit (`src/sessions/unified_session.cpp`) which supports *all* exchanges. This file is compiled once (~2-3 minutes).
*   **Benefit**: All exchange logic (`src/exchanges/*.cpp`) uses a lightweight wrapper. Modifying logic only triggers a ~3 second recompilation.

## Supported Exchanges & Status

| Exchange | Fetch Instruments | Notes |
| :--- | :--- | :--- |
| **AscendEX** | ✅ | Functional |
| **Binance** | ✅ (Blocked) | Logic correct, but Geo-blocked in CI/Cloud |
| **Binance US** | ✅ | Fixed via manual Generic Request |
| **Binance Coin Futures** | ✅ (Blocked) | Logic correct, but Geo-blocked in CI/Cloud |
| **Binance USDS Futures** | ✅ (Blocked) | Logic correct, but Geo-blocked in CI/Cloud (Error 451) |
| **Bitfinex** | ✅ | Functional |
| **Bitget** | ✅ | Functional |
| **Bitget Futures** | ✅ | Fixed via productType iteration |
| **Bitmart** | ✅ | Functional |
| **BitMEX** | ✅ | Fixed via manual Generic Request & Parsing |
| **Bitstamp** | ✅ | Functional |
| **Bybit** | ⚠️ | V5 Logic implemented, but Geo-blocked |
| **Coinbase** | ✅ | Functional |
| **Crypto.com** | ✅ | Functional |
| **Deribit** | ✅ | Functional (Options/Futures parsed) |
| **Gate.io** | ✅ | Functional |
| **Gate.io Perpetual** | ✅ | Fixed via settlement iteration |
| **Gemini** | ✅ | Functional |
| **Huobi** | ✅ | Functional |
| **Kraken** | ✅ | Functional |
| **Kraken Futures** | ✅ | Fixed via manual Generic Request & Parsing |
| **KuCoin** | ✅ | Functional |
| **KuCoin Futures** | ✅ | Functional |
| **MEXC** | ✅ | Functional |
| **OKX** | ✅ | Functional |
| **WhiteBIT** | ✅ | Functional |
| **ErisX** | ❌ | Disabled (Migrated to Cboe Digital) |

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
*   **OpenSSL** (System installed, e.g., `libssl-dev` on Debian/Ubuntu).
*   **Boost** (System installed or via `external/`).
*   **RapidJSON** (Included in `external/`).

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

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## Architecture

*   **`nccapi::Client`**: The entry point. Creates and holds the `UnifiedSession`.
*   **`nccapi::UnifiedSession`**: A wrapper around `ccapi::Session` that enables all supported exchanges. Compiled once.
*   **`nccapi::Exchange`**: Abstract base class. Concrete implementations (e.g., `Binance`) accept `UnifiedSession` via dependency injection.
*   **`src/exchanges/*.cpp`**: Lightweight logic implementations.
*   **`nccapi::Instrument`**: Polymorphic structure holding standardized instrument data (Spot, Future, Option details).
