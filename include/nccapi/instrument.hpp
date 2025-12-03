#ifndef NCCAPI_INSTRUMENT_HPP
#define NCCAPI_INSTRUMENT_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>

namespace nccapi {

struct Instrument {
    std::string id;             // Exchange-specific ID (e.g., "BTC-USD", "XXBTZUSD")
    std::string symbol;         // Normalized symbol (e.g., "BTC/USD")
    std::string base;           // Base asset (e.g., "BTC")
    std::string quote;          // Quote asset (e.g., "USD")
    double min_size = 0.0;      // Minimum order size
    double tick_size = 0.0;     // Price increment
    double step_size = 0.0;     // Quantity increment
    bool active = true;         // Trading status

    // Raw map for exchange-specific data
    std::map<std::string, std::string> info;

    // Helper for debugging
    std::string toString() const {
        return "Instrument(id=" + id + ", symbol=" + symbol + ", base=" + base + ", quote=" + quote + ")";
    }
};

} // namespace nccapi

#endif // NCCAPI_INSTRUMENT_HPP
