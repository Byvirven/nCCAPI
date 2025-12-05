#ifndef NCCAPI_INSTRUMENT_HPP
#define NCCAPI_INSTRUMENT_HPP

#include <string>
#include <map>
#include <sstream>

namespace nccapi {

/**
 * @brief Represents a trading instrument (pair/contract).
 */
struct Instrument {
    std::string id;             // Exchange-specific ID (e.g., "BTC-USDT", "XBTUSD")
    std::string symbol;         // Unified symbol (e.g., "BTC/USDT")
    std::string base;           // Base asset (e.g., "BTC")
    std::string quote;          // Quote asset (e.g., "USDT")
    std::string settle;         // Settlement asset (e.g., "USDT", "BTC")

    std::string type;           // spot, future, option, swap
    bool active = false;        // Trading status

    double tick_size = 0.0;     // Price increment
    double step_size = 0.0;     // Quantity increment
    double min_size = 0.0;      // Minimum quantity
    double min_notional = 0.0;  // Minimum notional value (price * qty)

    double contract_multiplier = 1.0; // Contract size/multiplier
    double contract_size = 0.0;       // Alternate size definition (e.g. 100 USD)

    // Derivatives / Options
    std::string underlying;     // Underlying asset (e.g., "BTC")
    std::string expiry;         // Expiration timestamp (ISO 8601 or ms string)
    double strike_price = 0.0;  // Strike price for options
    std::string option_type;    // "call" or "put"

    // Fees (if available)
    double maker_fee = 0.0;
    double taker_fee = 0.0;

    std::map<std::string, std::string> info; // Raw exchange info

    std::string toString() const {
        std::stringstream ss;
        ss << "Instrument(id=" << id
           << ", symbol=" << symbol
           << ", base=" << base
           << ", quote=" << quote
           << ", type=" << type;
        if (tick_size > 0) ss << ", tick=" << tick_size;
        if (step_size > 0) ss << ", step=" << step_size;
        ss << ")";
        return ss.str();
    }
};

} // namespace nccapi

#endif // NCCAPI_INSTRUMENT_HPP
