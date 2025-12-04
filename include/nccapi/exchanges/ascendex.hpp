#ifndef NCCAPI_EXCHANGES_ASCENDEX_HPP
#define NCCAPI_EXCHANGES_ASCENDEX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Ascendex : public Exchange {
public:
    Ascendex(std::shared_ptr<UnifiedSession> session);
    ~Ascendex() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "ascendex"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_ASCENDEX_HPP
