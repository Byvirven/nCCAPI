#include "nccapi/exchanges/deribit.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

namespace nccapi {

class Deribit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "deribit");

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_INSTRUMENTS) {
                            for (const auto& element : message.getElementList()) {
                                Instrument instrument;
                                instrument.id = element.getValue(CCAPI_INSTRUMENT);
                                // Deribit uses underlying symbol as base for options/futures often
                                instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string min_qty = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!min_qty.empty()) { try { instrument.min_size = std::stod(min_qty); } catch(...) {} }

                                if (element.has(CCAPI_UNDERLYING_SYMBOL)) {
                                    instrument.underlying = element.getValue(CCAPI_UNDERLYING_SYMBOL);
                                    if(instrument.base.empty()) instrument.base = instrument.underlying;
                                }
                                // Corrected field name
                                if (element.has(CCAPI_SETTLE_ASSET)) instrument.settle = element.getValue(CCAPI_SETTLE_ASSET);

                                if (element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                instrument.symbol = instrument.id; // Use ID as symbol for derivatives usually

                                // Determine type
                                if (instrument.id.find("-C") != std::string::npos || instrument.id.find("-P") != std::string::npos) {
                                    instrument.type = "option";
                                } else if (instrument.id.find("-PERPETUAL") != std::string::npos) {
                                    instrument.type = "swap";
                                } else {
                                    instrument.type = "future";
                                }

                                instrument.active = true; // Assume active if returned

                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            return instruments;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return instruments;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Deribit::Deribit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Deribit::~Deribit() = default;

std::vector<Instrument> Deribit::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
