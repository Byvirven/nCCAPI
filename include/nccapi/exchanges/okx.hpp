#ifndef NCCAPI_EXCHANGES_OKX_HPP
#define NCCAPI_EXCHANGES_OKX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Okx : public Exchange {
public:
    Okx();
    ~Okx() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "okx"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_OKX_HPP
