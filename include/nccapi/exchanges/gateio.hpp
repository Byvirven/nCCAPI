#ifndef NCCAPI_EXCHANGES_GATEIO_HPP
#define NCCAPI_EXCHANGES_GATEIO_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Gateio : public Exchange {
public:
    Gateio();
    ~Gateio() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "gateio"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_GATEIO_HPP
