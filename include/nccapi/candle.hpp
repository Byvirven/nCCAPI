#ifndef NCCAPI_CANDLE_HPP
#define NCCAPI_CANDLE_HPP

#include <string>
#include <sstream>
#include <iomanip>

namespace nccapi {

/**
 * @brief Represents a single OHLCV candlestick.
 */
struct Candle {
    uint64_t timestamp = 0; // Timestamp in milliseconds
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;

    std::string toString() const {
        std::stringstream ss;
        ss << "Candle(ts=" << timestamp
           << ", O=" << std::fixed << std::setprecision(8) << open
           << ", H=" << high
           << ", L=" << low
           << ", C=" << close
           << ", V=" << volume << ")";
        return ss.str();
    }
};

} // namespace nccapi

#endif // NCCAPI_CANDLE_HPP
