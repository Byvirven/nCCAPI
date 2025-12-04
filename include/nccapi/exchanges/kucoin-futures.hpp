#ifndef NCCAPI_EXCHANGES_KUCOIN_FUTURES_HPP
#define NCCAPI_EXCHANGES_KUCOIN_FUTURES_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class KucoinFutures : public Exchange {
public:
    KucoinFutures(std::shared_ptr<UnifiedSession> session);
    ~KucoinFutures() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "kucoin-futures"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_KUCOIN_FUTURES_HPP
