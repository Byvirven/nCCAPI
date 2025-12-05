# Architecture Overview

This project provides a standardized C++ wrapper for the [CCAPI (Crypto-Chassis)](https://github.com/crypto-chassis/ccapi) library. It is designed to offer a simple, unified interface (similar to CCXT) for interacting with multiple cryptocurrency exchanges, while maintaining high performance and fast compilation times.

## Core Components

### 1. Unified Session Strategy
To solve the significant build-time overhead associated with CCAPI's heavy template metaprogramming (which can take 45+ minutes to compile for 30+ exchanges), this project employs a **Unified Session** architecture.

*   **`src/sessions/unified_session.cpp`**: This single translation unit includes `ccapi_session.h` and enables **all** supported exchanges via macros (`CCAPI_ENABLE_EXCHANGE_...`). It is compiled once into a static library object.
*   **`include/nccapi/sessions/unified_session.hpp`**: A lightweight header file that defines the `UnifiedSession` class. This class wraps the underlying `ccapi::Session` pointer.
*   **Dependency Injection**: The `UnifiedSession` instance is created by the `Client` and passed to each exchange implementation.

This approach acts as a "compilation firewall," preventing the massive CCAPI headers from being parsed in every exchange's source file. Recompiling logic for a specific exchange now takes seconds instead of minutes.

### 2. Pimpl Idiom
Each exchange implementation (e.g., `Binance`, `Kraken`) uses the **Pimpl (Pointer to Implementation)** idiom.
*   **Header (`include/nccapi/exchanges/*.hpp`)**: Defines the public interface. It only forward-declares the `Impl` class and includes lightweight headers.
*   **Source (`src/exchanges/*.cpp`)**: Defines the `Impl` class, includes the `UnifiedSession` header, and implements the logic.

### 3. Generic Requests & Manual Parsing
While CCAPI provides standard request operations (e.g., `GET_INSTRUMENTS`), some exchanges have quirks or API changes that the standard implementation doesn't handle correctly (e.g., Binance US error -1104, Bitmex structure changes).
In these cases, the wrapper utilizes `GENERIC_PUBLIC_REQUEST` to manually construct the HTTP request and parse the JSON response using `rapidjson`. This ensures robustness and allows for quick fixes without waiting for upstream library updates.

### 4. Polymorphic Instrument Structure
The `nccapi::Instrument` structure is designed to handle Spot, Futures, Swaps, and Options uniformly.
*   It includes standard fields like `id`, `symbol`, `base`, `quote`.
*   It includes derivative-specific fields like `expiry`, `strike_price`, `option_type`, `settle`, `contract_size`.
*   It includes a raw `info` map to store all original data returned by the exchange for debugging or custom usage.

## Data Flow
1.  User instantiates `nccapi::Client`.
2.  `Client` creates a `std::shared_ptr<UnifiedSession>`.
3.  `Client` instantiates all Exchange objects, injecting the session.
4.  User calls `client.get_pairs("exchange")`.
5.  The specific Exchange implementation constructs a `ccapi::Request`.
6.  The request is sent via `UnifiedSession` (which delegates to `ccapi::Session`).
7.  The response events are parsed (either automatically by CCAPI service or manually) into `Instrument` objects.
8.  The vector of `Instrument` objects is returned to the user.
