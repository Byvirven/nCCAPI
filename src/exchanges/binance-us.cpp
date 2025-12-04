#include "nccapi/exchanges/binance-us.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/binance-us_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

// RapidJSON for manual parsing
#include "rapidjson/document.h"

namespace nccapi {

class BinanceUs::Impl {
public:
    Impl() {
        ccapi::SessionOptions options;
        ccapi::SessionConfigs configs;
        session = std::make_unique<BinanceUsSession>(options, configs);
    }

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;

        // Use GENERIC_PUBLIC_REQUEST to avoid CCAPI adding ?showPermissionSets=false (Error -1104)
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "binance-us", "", "GET_INSTRUMENTS");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v3/exchangeInfo"},
            {CCAPI_HTTP_METHOD, "GET"}
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GENERIC_PUBLIC_REQUEST) {
                            for (const auto& element : message.getElementList()) {
                                if (element.has(CCAPI_HTTP_BODY)) {
                                    std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_str.c_str());

                                    if (!doc.HasParseError() && doc.HasMember("symbols") && doc["symbols"].IsArray()) {
                                        for (const auto& s : doc["symbols"].GetArray()) {
                                            Instrument instrument;
                                            instrument.id = s["symbol"].GetString();
                                            instrument.base = s["baseAsset"].GetString();
                                            instrument.quote = s["quoteAsset"].GetString();

                                            // Status
                                            if (s.HasMember("status")) {
                                                instrument.active = (std::string(s["status"].GetString()) == "TRADING");
                                            }

                                            // Filters
                                            if (s.HasMember("filters") && s["filters"].IsArray()) {
                                                for (const auto& f : s["filters"].GetArray()) {
                                                    if (f.HasMember("filterType")) {
                                                        std::string type = f["filterType"].GetString();
                                                        if (type == "PRICE_FILTER") {
                                                            if (f.HasMember("tickSize")) instrument.tick_size = std::stod(f["tickSize"].GetString());
                                                        } else if (type == "LOT_SIZE") {
                                                            if (f.HasMember("stepSize")) instrument.step_size = std::stod(f["stepSize"].GetString());
                                                            if (f.HasMember("minQty")) instrument.min_size = std::stod(f["minQty"].GetString());
                                                        } else if (type == "NOTIONAL") {
                                                            if (f.HasMember("minNotional")) instrument.min_notional = std::stod(f["minNotional"].GetString());
                                                        }
                                                    }
                                                }
                                            }

                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }
                                            instrument.type = "spot";
                                            instruments.push_back(instrument);
                                        }
                                        return instruments;
                                    }
                                }
                            }
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
    std::unique_ptr<BinanceUsSession> session;
};

BinanceUs::BinanceUs() : pimpl(std::make_unique<Impl>()) {}
BinanceUs::~BinanceUs() = default;

std::vector<Instrument> BinanceUs::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
