#ifndef NCCAPI_EXCHANGES_BYBIT_HPP
#define NCCAPI_EXCHANGES_BYBIT_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Bybit : public Exchange {
public:
    Bybit();
    ~Bybit() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "bybit"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BYBIT_HPP
