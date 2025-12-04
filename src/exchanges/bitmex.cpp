#include "nccapi/exchanges/bitmex.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// CCAPI includes
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

class Bitmex::Impl {
public:
    Impl() {
        ccapi::SessionOptions options;
        ccapi::SessionConfigs configs;
        session = std::make_unique<ccapi::Session>(options, configs);
    }

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitmex");

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

                                // Bitmex specifics
                                if(element.has(CCAPI_UNDERLYING_SYMBOL)) instrument.underlying = element.getValue(CCAPI_UNDERLYING_SYMBOL);
                                if(element.has(CCAPI_CONTRACT_MULTIPLIER)) {
                                    std::string val = element.getValue(CCAPI_CONTRACT_MULTIPLIER);
                                    if(!val.empty()) { try { instrument.contract_multiplier = std::stod(val); } catch(...) {} }
                                }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id;
                                }

                                // Types
                                instrument.type = "future"; // Default for bitmex
                                if (instrument.id.find("ETHUSD") != std::string::npos && instrument.id.size() > 6) instrument.type = "swap"; // simplified check

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
    std::unique_ptr<ccapi::Session> session;
};

Bitmex::Bitmex() : pimpl(std::make_unique<Impl>()) {}
Bitmex::~Bitmex() = default;

std::vector<Instrument> Bitmex::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
