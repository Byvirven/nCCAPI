#ifndef NCCAPI_EXCHANGES_KUCOIN_HPP
#define NCCAPI_EXCHANGES_KUCOIN_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Kucoin : public Exchange {
public:
    Kucoin();
    ~Kucoin() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "kucoin"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_KUCOIN_HPP
