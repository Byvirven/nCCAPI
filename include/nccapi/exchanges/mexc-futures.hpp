#ifndef NCCAPI_EXCHANGES_MEXC_FUTURES_HPP
#define NCCAPI_EXCHANGES_MEXC_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class MexcFutures : public Exchange {
public:
    MexcFutures(std::shared_ptr<UnifiedSession> session);
    ~MexcFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "mexc-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_MEXC_FUTURES_HPP
