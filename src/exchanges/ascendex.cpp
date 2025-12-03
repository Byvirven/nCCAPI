#include "nccapi/exchanges/ascendex.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// CCAPI includes
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_EXCHANGE_ASCENDEX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

class Ascendex::Impl {
public:
    Impl() {
        ccapi::SessionOptions options;
        ccapi::SessionConfigs configs;
        session = std::make_unique<ccapi::Session>(options, configs);
    }

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "ascendex");

        // Specific fix for OKX if needed, or generic request
        // For now, standard request

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
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
                                if (!price_inc.empty()) instrument.tick_size = std::stod(price_inc);

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) instrument.step_size = std::stod(qty_inc);

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id; // Fallback
                                }

                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            // Log error but don't crash, return empty or what we have
                            // std::cerr << "Ascendex Error: " << message.getElementList()[0].getValue(CCAPI_ERROR_MESSAGE) << std::endl;
                            return instruments;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Timeout
        return instruments;
    }

private:
    std::unique_ptr<ccapi::Session> session;
};

Ascendex::Ascendex() : pimpl(std::make_unique<Impl>()) {}
Ascendex::~Ascendex() = default;

std::vector<Instrument> Ascendex::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
