#ifndef NCCAPI_EXCHANGES_BINANCE_HPP
#define NCCAPI_EXCHANGES_BINANCE_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Binance : public Exchange {
public:
    Binance();
    ~Binance() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "binance"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BINANCE_HPP
