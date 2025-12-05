#include "nccapi/exchanges/erisx.hpp"
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

class Erisx::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        // ErisX (now Cboe Digital) might need manual request
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "erisx", "", "GET_INSTRUMENTS");
        // Try alternate endpoint
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v3/products"},
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

                                            if (s.HasMember("symbol")) instrument.id = s["symbol"].GetString();
                                            else if (s.HasMember("id")) instrument.id = s["id"].GetString();
                                            else continue;

                                            // Basic parsing
                                            if (s.HasMember("baseCurrency")) instrument.base = s["baseCurrency"].GetString();
                                            if (s.HasMember("quoteCurrency")) instrument.quote = s["quoteCurrency"].GetString();

                                            // Symbol formatting
                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }

                                            if (s.HasMember("status")) {
                                                instrument.active = (std::string(s["status"].GetString()) == "OPEN");
                                            }
                                            instrument.type = "spot"; // Default or parse type

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

Erisx::Erisx(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Erisx::~Erisx() = default;

std::vector<Instrument> Erisx::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
