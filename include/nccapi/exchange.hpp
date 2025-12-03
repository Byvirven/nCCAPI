#ifndef NCCAPI_EXCHANGE_HPP
#define NCCAPI_EXCHANGE_HPP

#include <string>
#include <vector>
#include <memory>
#include "nccapi/instrument.hpp"

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

    // Future generic methods will go here
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGE_HPP
