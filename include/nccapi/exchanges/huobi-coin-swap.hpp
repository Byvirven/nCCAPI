#ifndef NCCAPI_EXCHANGES_HUOBI_COIN_SWAP_HPP
#define NCCAPI_EXCHANGES_HUOBI_COIN_SWAP_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class HuobiCoinSwap : public Exchange {
public:
    HuobiCoinSwap(std::shared_ptr<UnifiedSession> session);
    ~HuobiCoinSwap() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "huobi-coin-swap"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_HUOBI_COIN_SWAP_HPP
