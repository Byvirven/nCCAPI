#ifndef NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP
#define NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class GateioPerpetualFutures : public Exchange {
public:
    GateioPerpetualFutures();
    ~GateioPerpetualFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "gateio-perpetual-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_GATEIO_PERPETUAL_FUTURES_HPP
