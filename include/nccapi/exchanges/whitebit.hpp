#ifndef NCCAPI_EXCHANGES_WHITEBIT_HPP
#define NCCAPI_EXCHANGES_WHITEBIT_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class Whitebit : public Exchange {
public:
    Whitebit();
    ~Whitebit() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "whitebit"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_WHITEBIT_HPP
