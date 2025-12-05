#include "nccapi/exchanges/bybit.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

namespace nccapi {

class Bybit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        std::vector<std::string> categories = {"spot", "linear", "inverse", "option"};

        for (const auto& category : categories) {
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bybit");
            // CCAPI requires "category" param for Bybit
            request.appendParam({
                {CCAPI_INSTRUMENT_TYPE, category}
            });

            session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            bool received = false;
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
                std::vector<ccapi::Event> events = session->getEventQueue().purge();
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            if (message.getType() == ccapi::Message::Type::GET_INSTRUMENTS) {
                                for (const auto& element : message.getElementList()) {
                                    Instrument instrument;
                                    instrument.id = element.getValue(CCAPI_INSTRUMENT);
                                    instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                    instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                    std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                    if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                    std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                    if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                    std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                    if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }

                                    instrument.type = category;
                                    instrument.active = true;

                                    instruments.push_back(instrument);
                                }
                                received = true;
                            }
                        }
                    }
                }
                if (received) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return instruments;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Bybit::Bybit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bybit::~Bybit() = default;

std::vector<Instrument> Bybit::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
