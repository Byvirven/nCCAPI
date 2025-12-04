# nCCAPI - Normalized Crypto-Chassis API Wrapper

A standardized, header-only C++ wrapper for the [Crypto-Chassis CCAPI](https://github.com/crypto-chassis/ccapi) library. nCCAPI provides a unified interface similar to CCXT, allowing developers to interact with multiple cryptocurrency exchanges using a single, consistent API.

## Features

*   **Unified Interface**: Access multiple exchanges (Binance, Coinbase, Kraken, OKX, etc.) through a single `nccapi::Client`.
*   **Simplified Data Structures**: Standardized `Instrument` struct for trading pairs.
*   **Header-Only Wrapper**: Easy integration (though dependencies must be linked).
*   **Optimized Compilation**: Uses Pimpl and separate session compilation to drastically reduce build times during development.

## Build Time Optimization

This project employs a "Reduced Build Time" strategy. The heavy CCAPI template instantiations are isolated in `src/sessions/`, while the exchange logic resides in `src/exchanges/`.
*   **Benefit**: Modifying logic in `src/exchanges/` only triggers a 3-5 second recompilation instead of 1-2 minutes.
*   **Initial Build**: The first build will still be long as it must compile all session objects.

## Current Status (Refactoring Phase)

We are currently in a major refactoring phase to normalize the API.
**Completed:**
*   Global `get_instruments()` (Fetch Pairs) implementation for supported exchanges.
*   Factory pattern via `nccapi::Client`.
*   **Build Optimization**: Full split of Logic vs. CCAPI Session.

**Known Issues:**
*   **Binance (Global)**: Fully functional logic, but often geo-blocked in cloud/CI environments.
*   **Bybit**: Currently non-functional.
*   **Binance.US**: Currently non-functional.

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

*Note: Compilation may take significant time (up to 1 hour on limited hardware) due to the heavy template instantiation of the underlying CCAPI library.*

## Usage Example

```cpp
#include "nccapi/client.hpp"
#include <iostream>

int main() {
    try {
        // 1. Instantiate the Client (Factory)
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

*   **`nccapi::Client`**: The entry point. Manages exchange instances.
*   **`nccapi::Exchange`**: The abstract base class defining the interface.
*   **`src/exchanges/*.cpp`**: Lightweight logic implementations (Compile fast).
*   **`src/sessions/*.cpp`**: Heavy CCAPI session wrappers (Compile once, slow).
