#include "nccapi/exchanges/binance.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// CCAPI includes
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

class Binance::Impl {
public:
    Impl() {
        ccapi::SessionOptions options;
        ccapi::SessionConfigs configs;
        session = std::make_unique<ccapi::Session>(options, configs);
    }

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "binance");

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
                                instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                if (element.has(CCAPI_ORDER_QUOTE_QUANTITY_MIN)) {
                                    std::string val = element.getValue(CCAPI_ORDER_QUOTE_QUANTITY_MIN);
                                    if(!val.empty()) { try { instrument.min_notional = std::stod(val); } catch(...) {} }
                                }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id;
                                }
                                instrument.type = "spot";

                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "TRADING");
                                }

                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                             // Log error if needed, or return empty
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
    std::unique_ptr<ccapi::Session> session;
};

Binance::Binance() : pimpl(std::make_unique<Impl>()) {}
Binance::~Binance() = default;

std::vector<Instrument> Binance::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
