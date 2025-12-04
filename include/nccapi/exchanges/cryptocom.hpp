#ifndef NCCAPI_EXCHANGES_CRYPTOCOM_HPP
#define NCCAPI_EXCHANGES_CRYPTOCOM_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Cryptocom : public Exchange {
public:
    Cryptocom(std::shared_ptr<UnifiedSession> session);
    ~Cryptocom() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "cryptocom"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_CRYPTOCOM_HPP
