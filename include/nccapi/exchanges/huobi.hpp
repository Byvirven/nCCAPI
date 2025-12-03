#ifndef NCCAPI_EXCHANGES_HUOBI_HPP
#define NCCAPI_EXCHANGES_HUOBI_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Huobi : public Exchange {
public:
    Huobi();
    ~Huobi() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "huobi"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_HUOBI_HPP
