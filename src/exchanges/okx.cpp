#include "nccapi/exchanges/okx.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

namespace nccapi {

class Okx::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> all_instruments;
        std::vector<std::string> instTypes = {"SPOT", "SWAP", "FUTURES", "OPTION"};

        for (const auto& instType : instTypes) {
            std::vector<Instrument> batch = fetch_instruments_by_type(instType);
            all_instruments.insert(all_instruments.end(), batch.begin(), batch.end());
            // Small delay to avoid rate limits
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        return all_instruments;
    }

private:
    std::shared_ptr<UnifiedSession> session;

    std::vector<Instrument> fetch_instruments_by_type(const std::string& instType) {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "okx");
        request.appendParam({{"instType", instType}});

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
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET); // Note: For Futures/Options, might be empty or settlement currency

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                // Normalized Symbol
                                if (instType == "SPOT") {
                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "spot";
                                } else {
                                    instrument.symbol = instrument.id; // Keep original ID for derivatives
                                    if (instType == "SWAP") instrument.type = "swap";
                                    else if (instType == "FUTURES") instrument.type = "future";
                                    else if (instType == "OPTION") instrument.type = "option";
                                }

                                // Status
                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "live");
                                }

                                // Derivative fields
                                if (element.has(CCAPI_CONTRACT_SIZE)) {
                                    std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                    if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }
                                if (element.has(CCAPI_CONTRACT_MULTIPLIER)) {
                                    std::string val = element.getValue(CCAPI_CONTRACT_MULTIPLIER);
                                    if(!val.empty()) { try { instrument.contract_multiplier = std::stod(val); } catch(...) {} }
                                }
                                if (element.has(CCAPI_UNDERLYING_SYMBOL)) instrument.underlying = element.getValue(CCAPI_UNDERLYING_SYMBOL);
                                if (element.has(CCAPI_SETTLE_ASSET)) instrument.settle_asset = element.getValue(CCAPI_SETTLE_ASSET);

                                // Store raw info
                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                             // std::cerr << "Okx Error (" << instType << "): " << message.getElementList()[0].getValue(CCAPI_ERROR_MESSAGE) << std::endl;
                             return instruments;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return instruments;
    }
};

Okx::Okx(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Okx::~Okx() = default;

std::vector<Instrument> Okx::get_instruments() {
    return pimpl->get_instruments();
}

} // namespace nccapi
