#ifndef NCCAPI_EXCHANGES_BITGET_HPP
#define NCCAPI_EXCHANGES_BITGET_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Bitget : public Exchange {
public:
    Bitget();
    ~Bitget() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "bitget"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITGET_HPP
