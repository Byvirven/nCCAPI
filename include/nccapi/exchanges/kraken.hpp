#ifndef NCCAPI_EXCHANGES_KRAKEN_HPP
#define NCCAPI_EXCHANGES_KRAKEN_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Kraken : public Exchange {
public:
    Kraken();
    ~Kraken() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "kraken"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_KRAKEN_HPP
