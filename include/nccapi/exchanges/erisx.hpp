#ifndef NCCAPI_EXCHANGES_ERISX_HPP
#define NCCAPI_EXCHANGES_ERISX_HPP

#include "nccapi/exchange.hpp"
#include <memory>

namespace nccapi {

class UnifiedSession;

class Erisx : public Exchange {
public:
    Erisx(std::shared_ptr<UnifiedSession> session);
    ~Erisx() override;

    std::vector<Instrument> get_instruments() override;
    std::string get_name() const override { return "erisx"; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace nccapi

#endif // NCCAPI_EXCHANGES_ERISX_HPP
