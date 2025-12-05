# nCCAPI - Normalized Crypto-Chassis API Wrapper

A standardized, header-only C++ wrapper for the [Crypto-Chassis CCAPI](https://github.com/crypto-chassis/ccapi) library. nCCAPI provides a unified interface similar to CCXT, allowing developers to interact with multiple cryptocurrency exchanges using a single, consistent API.

## Features

*   **Unified Interface**: Access multiple exchanges (Binance, Coinbase, Kraken, OKX, etc.) through a single `nccapi::Client`.
*   **Simplified Data Structures**: Standardized `Instrument` struct for trading pairs.
*   **Header-Only Wrapper**: Easy integration (though dependencies must be linked).
*   **Optimized Compilation**: Uses Pimpl and a **Unified Session** architecture to keep build times extremely low during development.

## Build Time Optimization

This project employs a "Unified Session" strategy.
*   **Problem**: CCAPI is header-only and uses heavy template meta-programming. Compiling support for 30+ exchanges usually takes 45+ minutes.
*   **Solution**: We isolate the CCAPI session instantiation into a single translation unit (`src/sessions/unified_session.cpp`) which supports *all* exchanges. This file is compiled once (~2-3 minutes).
*   **Benefit**: All exchange logic (`src/exchanges/*.cpp`) uses a lightweight wrapper. Modifying logic only triggers a ~3 second recompilation.

## Current Status (Refactoring Phase)

We are currently in a major refactoring phase to normalize the API.
**Completed:**
*   Global `get_instruments()` (Fetch Pairs) implementation for supported exchanges.
*   Factory pattern via `nccapi::Client`.
*   **Architecture**: Transitioned to `UnifiedSession` (Shared Session) model.
*   **Binance US**: Fixed `get_instruments` by using manual Generic Requests to bypass API limitations.
*   **Bitmex**: Fixed crash by using manual Generic Requests and safe JSON parsing.
*   **Bitget Futures**: Fixed empty instrument list by iterating over required product types.

**Known Issues:**
*   **Binance (Global)**: Fully functional logic, but often geo-blocked in cloud/CI environments.
*   **Bybit**: Currently non-functional.

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
            std::cout << "- " << inst.symbol << " (Base: " << inst.base_asset
                      << ", Quote: " << inst.quote_asset << ")" << std::endl;
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
