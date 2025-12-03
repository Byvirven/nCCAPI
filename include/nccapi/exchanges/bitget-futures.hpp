#ifndef NCCAPI_EXCHANGES_BITGET_FUTURES_HPP
#define NCCAPI_EXCHANGES_BITGET_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class BitgetFutures : public Exchange {
public:
    BitgetFutures();
    ~BitgetFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "bitget-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITGET_FUTURES_HPP
