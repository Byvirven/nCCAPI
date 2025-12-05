#include "nccapi/exchanges/kraken-futures.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

#include "rapidjson/document.h"

namespace nccapi {

class KrakenFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken-futures", "", "GET_INSTRUMENTS");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/derivatives/api/v3/instruments"},
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("instruments") && doc["instruments"].IsArray()) {
                                        for (const auto& s : doc["instruments"].GetArray()) {
                                            Instrument instrument;

                                            if (s.HasMember("symbol")) instrument.id = s["symbol"].GetString();
                                            else continue;

                                            // Basic parsing
                                            if (s.HasMember("underlying")) instrument.base = s["underlying"].GetString(); // Simplified
                                            instrument.quote = "USD"; // Mostly USD?
                                            // Kraken futures symbols are complex (e.g., pi_xbtusd).
                                            // Attempt to parse symbol for base/quote if underlying is simple

                                            if (s.HasMember("tickSize")) instrument.tick_size = s["tickSize"].GetDouble();
                                            if (s.HasMember("contractSize")) instrument.contract_multiplier = s["contractSize"].GetDouble();

                                            // Symbol formatting
                                            instrument.symbol = instrument.id;

                                            if (s.HasMember("tradeable")) {
                                                instrument.active = s["tradeable"].GetBool();
                                            }

                                            instrument.type = "future";
                                            if (s.HasMember("type")) instrument.type = s["type"].GetString();

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
    std::shared_ptr<UnifiedSession> session;
};

KrakenFutures::KrakenFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
KrakenFutures::~KrakenFutures() = default;

std::vector<Instrument> KrakenFutures::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
