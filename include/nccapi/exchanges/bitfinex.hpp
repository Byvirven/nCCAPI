#ifndef NCCAPI_EXCHANGES_BITFINEX_HPP
#define NCCAPI_EXCHANGES_BITFINEX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Bitfinex : public Exchange {
public:
    Bitfinex(std::shared_ptr<UnifiedSession> session);
    ~Bitfinex() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "bitfinex"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITFINEX_HPP
