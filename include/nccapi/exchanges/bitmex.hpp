#ifndef NCCAPI_EXCHANGES_BITMEX_HPP
#define NCCAPI_EXCHANGES_BITMEX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Bitmex : public Exchange {
public:
    Bitmex();
    ~Bitmex() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "bitmex"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_BITMEX_HPP
