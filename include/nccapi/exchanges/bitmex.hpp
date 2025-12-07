#ifndef NCCAPI_EXCHANGES_BITMEX_HPP
#define NCCAPI_EXCHANGES_BITMEX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Bitmex : public Exchange {
public:
    Bitmex(std::shared_ptr<UnifiedSession> session);
    ~Bitmex() override;

    std::vector<Instrument> get_instruments() override;
    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) override;
    std::string get_name() const override { return "bitmex"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITMEX_HPP
