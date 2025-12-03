#ifndef NCCAPI_CLIENT_HPP
#define NCCAPI_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "nccapi/instrument.hpp"
#include "nccapi/exchange.hpp"

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
