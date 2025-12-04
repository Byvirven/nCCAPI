#ifndef NCCAPI_INSTRUMENT_HPP
#define NCCAPI_INSTRUMENT_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <optional>

namespace nccapi {

/**
 * @brief Unified Instrument structure compatible with all CCAPI supported exchanges.
 * Supports Spot, Futures, Options, and Swaps.
 */
struct Instrument {
    // Identity
    std::string id;             // Exchange-specific ID (e.g., "BTC-USD", "XXBTZUSD", "BTC-25SEP20")
    std::string symbol;         // Normalized symbol (e.g., "BTC/USD")
    std::string base;           // Base asset (e.g., "BTC")
    std::string quote;          // Quote asset (e.g., "USD", "USDT")

    // Status
    bool active = true;         // Trading status

    // Order Constraints
    double min_size = 0.0;      // Minimum order quantity (e.g., 0.001 BTC)
    double min_notional = 0.0;  // Minimum order value in quote currency (e.g., 5 USDT)
    double tick_size = 0.0;     // Price increment (e.g., 0.01)
    double step_size = 0.0;     // Quantity increment (e.g., 0.00001)

    // Derivatives (Futures/Swaps/Options)
    double contract_size = 1.0;          // Size of one contract (e.g., 100 USD, 1 BTC)
    double contract_multiplier = 1.0;    // Multiplier for value calculation
    std::string underlying;              // Underlying asset symbol (e.g., "BTC")
    std::string settle_asset;            // Settlement asset (e.g., "USDT", "BTC")
    std::string expire_date;             // Expiration date (ISO 8601 or exchange specific)
    std::string type;                    // "spot", "future", "option", "swap"

    // Raw map for exchange-specific data (everything else)
    std::map<std::string, std::string> info;

    // Helper for debugging
    std::string toString() const {
        return "Instrument(id=" + id + ", symbol=" + symbol + ", base=" + base + ", quote=" + quote +
               ", tick=" + std::to_string(tick_size) + ", step=" + std::to_string(step_size) + ")";
    }
};

} // namespace nccapi

#endif // NCCAPI_INSTRUMENT_HPP
