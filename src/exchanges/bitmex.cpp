#include "nccapi/exchanges/bitmex.hpp"
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

class Bitmex::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        // Use Generic Request to avoid CCAPI Service Crash on missing fields
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitmex", "", "GET_INSTRUMENTS");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v1/instrument/active"},
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& s : doc.GetArray()) {
                                            Instrument instrument;
                                            if (!s.HasMember("symbol")) continue;

                                            instrument.id = s["symbol"].GetString();

                                            // Base/Quote
                                            if (s.HasMember("rootSymbol")) instrument.base = s["rootSymbol"].GetString();
                                            if (s.HasMember("quoteCurrency")) instrument.quote = s["quoteCurrency"].GetString();

                                            // Tick/Step
                                            if (s.HasMember("tickSize")) instrument.tick_size = s["tickSize"].GetDouble();
                                            if (s.HasMember("lotSize")) instrument.step_size = s["lotSize"].GetDouble();

                                            // Multiplier
                                            if (s.HasMember("multiplier")) instrument.contract_multiplier = s["multiplier"].GetDouble();

                                            // Symbol
                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }

                                            // Type
                                            if (s.HasMember("typ")) {
                                                std::string typ = s["typ"].GetString();
                                                if (typ == "FFWCSX") instrument.type = "swap";
                                                else if (typ == "FFWCCS") instrument.type = "future";
                                                else instrument.type = typ;
                                            } else {
                                                instrument.type = "future";
                                            }

                                            // Active (we called /active so safe to assume, but check state)
                                            if (s.HasMember("state")) {
                                                instrument.active = (std::string(s["state"].GetString()) == "Open");
                                            }

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

Bitmex::Bitmex(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bitmex::~Bitmex() = default;

std::vector<Instrument> Bitmex::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
