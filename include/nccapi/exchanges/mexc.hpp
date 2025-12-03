#ifndef NCCAPI_EXCHANGES_MEXC_HPP
#define NCCAPI_EXCHANGES_MEXC_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Mexc : public Exchange {
public:
    Mexc();
    ~Mexc() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "mexc"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_MEXC_HPP
