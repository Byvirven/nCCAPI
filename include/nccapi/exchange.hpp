#ifndef NCCAPI_EXCHANGE_HPP
#define NCCAPI_EXCHANGE_HPP

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "nccapi/instrument.hpp"
#include "nccapi/candle.hpp"

namespace nccapi {

/**
 * @brief Abstract base class for all exchanges.
 * Uses Pimpl idiom to hide CCAPI implementation details.
 */
class Exchange {
public:
    virtual ~Exchange() = default;

    /**
     * @brief Fetch all available instruments (pairs) from the exchange.
     * @return std::vector<Instrument> List of instruments.
     */
    virtual std::vector<Instrument> get_instruments() = 0;

    /**
     * @brief Get the exchange name.
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Get historical candles (OHLCV) for a specific instrument.
     * @param instrument_name The instrument identifier (e.g., "BTC-USDT").
     * @param timeframe The time interval (e.g., "1m", "1h"). Default is "1m".
     * @param from_date Start timestamp in milliseconds.
     * @param to_date End timestamp in milliseconds.
     * @return std::vector<Candle> List of candles.
     */
    virtual std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                                       const std::string& timeframe,
                                                       int64_t from_date,
                                                       int64_t to_date) {
        throw std::runtime_error("get_historical_candles not implemented for " + get_name());
    }

    // Future generic methods will go here
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGE_HPP
