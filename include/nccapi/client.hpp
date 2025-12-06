#ifndef NCCAPI_CLIENT_HPP
#define NCCAPI_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "nccapi/instrument.hpp"
#include "nccapi/exchange.hpp"
#include "nccapi/candle.hpp"

namespace nccapi {

/**
 * @brief Main entry point (Facade) for the nCCAPI library.
 */
class Client {
public:
    Client();
    ~Client();

    /**
     * @brief Get a list of supported exchanges.
     */
    std::vector<std::string> get_supported_exchanges() const;

    /**
     * @brief Generic function to get pairs (instruments) from any exchange.
     * @param exchange_name The name of the exchange (e.g., "coinbase", "binance").
     * @return List of instruments. Throws if exchange is not supported or on error.
     */
    std::vector<Instrument> get_pairs(const std::string& exchange_name);

    /**
     * @brief Generic function to get historical candles from any exchange.
     * @param exchange_name The name of the exchange.
     * @param instrument_name The instrument identifier.
     * @param timeframe The time interval (default "1m").
     * @param from_date Start timestamp in milliseconds (0 for exchange default).
     * @param to_date End timestamp in milliseconds (0 for now).
     * @return List of candles.
     */
    std::vector<Candle> get_historical_candles(const std::string& exchange_name,
                                               const std::string& instrument_name,
                                               const std::string& timeframe = "1m",
                                               int64_t from_date = 0,
                                               int64_t to_date = 0);

    /**
     * @brief Access the specific exchange instance directly if needed.
     */
    std::shared_ptr<Exchange> get_exchange(const std::string& exchange_name);

private:
    // Map of exchange name to Exchange instance
    std::map<std::string, std::shared_ptr<Exchange>> exchanges_;

    void load_exchange(const std::string& exchange_name);
};

} // namespace nccapi

#endif // NCCAPI_CLIENT_HPP
