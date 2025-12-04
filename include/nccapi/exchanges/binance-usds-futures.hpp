#ifndef NCCAPI_EXCHANGES_BINANCE_USDS_FUTURES_HPP
#define NCCAPI_EXCHANGES_BINANCE_USDS_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class BinanceUsdsFutures : public Exchange {
public:
    BinanceUsdsFutures(std::shared_ptr<UnifiedSession> session);
    ~BinanceUsdsFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "binance-usds-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BINANCE_USDS_FUTURES_HPP
