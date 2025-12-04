#ifndef NCCAPI_EXCHANGES_BINANCE_COIN_FUTURES_HPP
#define NCCAPI_EXCHANGES_BINANCE_COIN_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class BinanceCoinFutures : public Exchange {
public:
    BinanceCoinFutures(std::shared_ptr<UnifiedSession> session);
    ~BinanceCoinFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "binance-coin-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BINANCE_COIN_FUTURES_HPP
