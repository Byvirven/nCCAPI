#ifndef NCCAPI_EXCHANGES_BINANCE_HPP
#define NCCAPI_EXCHANGES_BINANCE_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Binance : public Exchange {
public:
    Binance(std::shared_ptr<UnifiedSession> session);
    ~Binance() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "binance"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BINANCE_HPP
