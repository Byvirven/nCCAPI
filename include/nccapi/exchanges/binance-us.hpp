#ifndef NCCAPI_EXCHANGES_BINANCE_US_HPP
#define NCCAPI_EXCHANGES_BINANCE_US_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class BinanceUs : public Exchange {
public:
    BinanceUs(std::shared_ptr<UnifiedSession> session);
    ~BinanceUs() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "binance-us"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BINANCE_US_HPP
