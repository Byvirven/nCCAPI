#ifndef NCCAPI_EXCHANGES_BITGET_HPP
#define NCCAPI_EXCHANGES_BITGET_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Bitget : public Exchange {
public:
    Bitget(std::shared_ptr<UnifiedSession> session);
    ~Bitget() override;

    std::vector<Instrument> get_instruments() override;
    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) override;
    std::string get_name() const override { return "bitget"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITGET_HPP
