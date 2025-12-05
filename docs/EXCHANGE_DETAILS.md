# Exchange Details & Quirks

This document details the implementation status and specific handling for supported exchanges.

## Exchange Status

| Exchange | Status | Fetch Instruments Method | Notes |
| :--- | :--- | :--- | :--- |
| **AscendEX** | ✅ | Standard | |
| **Binance** | ✅ | Standard | Geo-blocked in some cloud environments. |
| **Binance US** | ✅ | **Manual Generic Request** | Uses `/api/v3/exchangeInfo` to avoid `-1104` error caused by standard CCAPI param injection. |
| **Binance Coin Futures** | ✅ | Standard | Geo-blocked in some cloud environments. |
| **Binance USDS Futures** | ✅ | Standard | Geo-blocked (Error 451) in restricted regions. |
| **Bitfinex** | ✅ | Standard | |
| **Bitget** | ✅ | Standard | |
| **Bitget Futures** | ✅ | **Standard with Param Loop** | Iterates over `productType` (USDT-FUTURES, COIN-FUTURES, USDC-FUTURES) as required by V2 API. |
| **Bitmart** | ✅ | Standard | |
| **BitMEX** | ✅ | **Manual Generic Request** | Uses `/api/v1/instrument/active` with manual JSON parsing to avoid assertion crashes in default service. |
| **Bitstamp** | ✅ | Standard | |
| **Bybit** | ⚠️ | **Manual Generic Request** | Implements V5 API logic (iterating categories: spot, linear, inverse, option). Validation blocked by Geofencing. |
| **Coinbase** | ✅ | Standard | |
| **Crypto.com** | ✅ | Standard | |
| **Deribit** | ✅ | Standard | Parses `instrument_name` for options/futures details. |
| **Gate.io** | ✅ | Standard | |
| **Gate.io Perpetual** | ✅ | **Standard with Param Loop** | Iterates over `settle` currencies (usdt, btc, usd) to fetch all contracts. |
| **Gemini** | ✅ | Standard | |
| **Huobi** | ✅ | Standard | |
| **Kraken** | ✅ | Standard | |
| **Kraken Futures** | ✅ | **Manual Generic Request** | Uses `/derivatives/api/v3/instruments` with manual parsing to avoid schema mismatches. |
| **KuCoin** | ✅ | Standard | |
| **KuCoin Futures** | ✅ | Standard | |
| **MEXC** | ✅ | Standard | |
| **MEXC Futures** | ✅ | Standard | |
| **OKX** | ✅ | **Standard with Param Loop** | Iterates `instType` (SPOT, SWAP, FUTURES, OPTION). |
| **WhiteBIT** | ✅ | Standard | |
| **ErisX** | ❌ | **Removed** | Exchange migrated to Cboe Digital. Support removed. |

## Key Implementation Quirks

### Manual Generic Requests
For exchanges where the upstream CCAPI service definition is outdated or incompatible with specific endpoints (e.g., due to extra parameters causing errors, or strict JSON schema validation failing), this wrapper bypasses the service logic.
It constructs a raw HTTP request (`GENERIC_PUBLIC_REQUEST`) and uses `rapidjson` to parse the response body directly. This provides maximum resilience and control.

**Exchanges using this:**
- Binance US
- Bitmex
- Kraken Futures
- Bybit (V5)

### Parameter Iteration
Some exchanges require a specific parameter to filter instruments (e.g., "Product Type" or "Settlement Currency") and do not support a "fetch all" wildcard. In these cases, the wrapper iterates through a known list of common values to aggregate the full list.

**Exchanges using this:**
- Bitget Futures (`productType`)
- Gate.io Perpetual (`settle`)
- OKX (`instType`)
- Huobi (`contract_code` usually implied, but handled via standard service)
