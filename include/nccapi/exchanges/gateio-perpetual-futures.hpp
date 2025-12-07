#ifndef NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP
#define NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class GateioPerpetualFutures : public Exchange {
public:
    GateioPerpetualFutures(std::shared_ptr<UnifiedSession> session);
    ~GateioPerpetualFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) override;
    std::string get_name() const override { return "gateio-perpetual-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP
