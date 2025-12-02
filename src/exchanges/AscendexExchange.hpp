#pragma once

#include "GenericExchange.hpp"
#include <vector>
#include <string>
#include <map>

namespace unified_crypto {

class AscendexExchange : public GenericExchange {
public:
    AscendexExchange(const std::string& exchange, const ExchangeConfig& config = {})
        : GenericExchange(exchange, config) {}

    std::vector<Instrument> fetchInstruments() override {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Instruments AscendEX");
        // AscendEX uses "cash" for spot products
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/cash/products"}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
             if (event.getType() == ccapi::Event::Type::RESPONSE) {
                 for(const auto& msg : event.getMessageList()) {
                     for(const auto& elem : msg.getElementList()) {
                         if(elem.getNameValueMap().count("HTTP_BODY")) {
                             std::string body = elem.getNameValueMap().at("HTTP_BODY");
                             // std::cout << "DEBUG BODY: " << body << std::endl;
                             rapidjson::Document d; d.Parse(body.c_str());
                             if(!d.HasParseError() && d.HasMember("data")) {
                                 const auto& arr = d["data"];
                                 if (arr.Size() > 0) {
                                     // std::cout << "DEBUG FIRST ITEM: " << "..." << std::endl; // Commented out to avoid clutter if not needed
                                 }
                                 for(const auto& i : arr.GetArray()) {
                                     Instrument inst;
                                     if (i.HasMember("symbol")) inst.symbol = i["symbol"].GetString();
                                     if (i.HasMember("baseAsset")) inst.baseAsset = i["baseAsset"].GetString();
                                     if (i.HasMember("quoteAsset")) inst.quoteAsset = i["quoteAsset"].GetString();
                                     if (inst.baseAsset.empty() && !inst.symbol.empty()) {
                                         size_t pos = inst.symbol.find('/');
                                         if (pos != std::string::npos) {
                                             inst.baseAsset = inst.symbol.substr(0, pos);
                                             inst.quoteAsset = inst.symbol.substr(pos + 1);
                                         }
                                     }
                                     if (i.HasMember("statusCode")) inst.status = i["statusCode"].GetString();
                                     if(i.HasMember("tickSize")) inst.tickSize = std::stod(i["tickSize"].GetString());
                                     if(i.HasMember("minQty")) inst.minSize = std::stod(i["minQty"].GetString());
                                     if(i.HasMember("lotSize")) inst.stepSize = std::stod(i["lotSize"].GetString());
                                     instruments.push_back(inst);
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return instruments;
    }

    Instrument fetchInstrument(const std::string& symbol) override {
        Instrument inst; inst.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Instrument AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/cash/products"}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
             if (event.getType() == ccapi::Event::Type::RESPONSE) {
                 for(const auto& msg : event.getMessageList()) {
                     for(const auto& elem : msg.getElementList()) {
                         if(elem.getNameValueMap().count("HTTP_BODY")) {
                             std::string body = elem.getNameValueMap().at("HTTP_BODY");
                             rapidjson::Document d; d.Parse(body.c_str());
                             if(!d.HasParseError() && d.HasMember("data")) {
                                 for(const auto& i : d["data"].GetArray()) {
                                     if(i["symbol"].GetString() == symbol) {
                                         if (i.HasMember("baseAsset")) inst.baseAsset = i["baseAsset"].GetString();
                                         if (i.HasMember("quoteAsset")) inst.quoteAsset = i["quoteAsset"].GetString();
                                         if (inst.baseAsset.empty() && !inst.symbol.empty()) {
                                             size_t pos = inst.symbol.find('/');
                                             if (pos != std::string::npos) {
                                                 inst.baseAsset = inst.symbol.substr(0, pos);
                                                 inst.quoteAsset = inst.symbol.substr(pos + 1);
                                             }
                                         }
                                         if (i.HasMember("statusCode")) inst.status = i["statusCode"].GetString();
                                         if(i.HasMember("tickSize")) inst.tickSize = std::stod(i["tickSize"].GetString());
                                         if(i.HasMember("minQty")) inst.minSize = std::stod(i["minQty"].GetString());
                                         if(i.HasMember("lotSize")) inst.stepSize = std::stod(i["lotSize"].GetString());
                                         break;
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return inst;
    }

    Ticker fetchTicker(const std::string& symbol) override {
        Ticker ticker; ticker.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/spot/ticker"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            const auto& data = d["data"];
                            if(data.IsObject()) {
                                if(data.HasMember("close")) ticker.lastPrice = std::stod(data["close"].GetString());
                                if(data.HasMember("bid")) {
                                    ticker.bidPrice = std::stod(data["bid"].GetArray()[0].GetString());
                                    ticker.bidSize = std::stod(data["bid"].GetArray()[1].GetString());
                                }
                                if(data.HasMember("ask")) {
                                    ticker.askPrice = std::stod(data["ask"].GetArray()[0].GetString());
                                    ticker.askSize = std::stod(data["ask"].GetArray()[1].GetString());
                                }
                                ticker.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); // Approximate
                            }
                        }
                    }
                }
            }
        }
        if (ticker.lastPrice == 0.0 && ticker.bidPrice > 0) ticker.lastPrice = (ticker.bidPrice + ticker.askPrice)/2.0;
        return ticker;
    }

    OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) override {
        OrderBook ob; ob.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Book AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/depth"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            const auto& data = d["data"]["data"];
                            ob.timestamp = msToIso(data["ts"].GetInt64());
                            for(const auto& b : data["bids"].GetArray()) ob.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                            for(const auto& a : data["asks"].GetArray()) ob.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                        }
                    }
                }
            }
        }
        return ob;
    }

    std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) override {
        std::vector<Trade> trades;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Trades AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/trades"}, {"HTTP_QUERY_STRING", "symbol=" + symbol + "&n=" + std::to_string(limit)}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            const auto& data = d["data"]["data"];
                            if(data.IsArray()) {
                                for(const auto& t_json : data.GetArray()) {
                                    Trade t;
                                    t.symbol = symbol;
                                    t.id = std::to_string(t_json["seqnum"].GetInt64());
                                    t.price = std::stod(t_json["p"].GetString());
                                    t.size = std::stod(t_json["q"].GetString());
                                    t.timestamp = msToIso(t_json["ts"].GetInt64());
                                    t.isBuyerMaker = t_json["bm"].GetBool();
                                    t.side = t.isBuyerMaker ? "sell" : "buy";
                                    trades.push_back(t);
                                }
                            }
                        }
                    }
                }
            }
        }
        return trades;
    }

    std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) override {
        return fetchOHLCVHistorical(symbol, timeframe, "", "", limit);
    }

    std::vector<OHLCV> fetchOHLCVHistorical(const std::string& symbol, const std::string& timeframe, const std::string& startTime, const std::string& endTime, int limit) override {
        std::vector<OHLCV> candles;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OHLCV AscendEX");

        std::string interval = "1"; // 1 minute default
        if (timeframe == "60") interval = "1";
        else if (timeframe == "3600") interval = "60";
        else if (timeframe == "86400") interval = "1d";

        std::string qs = "symbol=" + symbol + "&interval=" + interval;
        if (limit > 0) qs += "&n=" + std::to_string(limit);
        if (!startTime.empty()) qs += "&from=" + isoToMs(startTime);
        if (!endTime.empty()) qs += "&to=" + isoToMs(endTime);

        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/barhist"}, {"HTTP_QUERY_STRING", qs}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            for(const auto& item : d["data"].GetArray()) {
                                const auto& k = item["data"];
                                OHLCV c;
                                c.timestamp = msToIso(k["ts"].GetInt64());
                                c.open = std::stod(k["o"].GetString());
                                c.high = std::stod(k["h"].GetString());
                                c.low = std::stod(k["l"].GetString());
                                c.close = std::stod(k["c"].GetString());
                                c.volume = std::stod(k["v"].GetString());
                                candles.push_back(c);
                            }
                        }
                    }
                }
            }
        }
        return candles;
    }

    std::vector<Trade> fetchTradesHistorical(const std::string& symbol, const std::string& startTime, const std::string& endTime, int limit) override {
        // AscendEX doesn't support time-based trade history public endpoint easily.
        // Returning recent trades as fallback.
        return fetchTrades(symbol, limit);
    }

    TickerStats fetchTicker24h(const std::string& symbol) override {
        TickerStats stats; stats.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/spot/ticker"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            const auto& data = d["data"];
                            if(data.IsObject()) {
                                if(data.HasMember("close")) stats.lastPrice = std::stod(data["close"].GetString());
                                if(data.HasMember("open")) stats.openPrice = std::stod(data["open"].GetString());
                                if(data.HasMember("high")) stats.highPrice = std::stod(data["high"].GetString());
                                if(data.HasMember("low")) stats.lowPrice = std::stod(data["low"].GetString());
                                if(data.HasMember("volume")) stats.volume = std::stod(data["volume"].GetString());
                                // AscendEx ticker doesn't provide priceChange/Percent explicitly, can be calculated.
                                if (stats.openPrice > 0) {
                                    stats.priceChange = stats.lastPrice - stats.openPrice;
                                    stats.priceChangePercent = (stats.priceChange / stats.openPrice) * 100.0;
                                }
                            }
                        }
                    }
                }
            }
        }
        return stats;
    }

    long long fetchServerTime() override {
        // Fallback to local time if no endpoint found, or use exchange-info
        // GET /api/pro/v1/exchange-info
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Server Time AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/exchange-info"}, {"HTTP_QUERY_STRING", "requestTime=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())}});

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data") && d["data"].HasMember("requestReceiveAt")) {
                            return d["data"]["requestReceiveAt"].GetInt64();
                        }
                    }
                }
            }
        }
        return 0;
    }

    // Private API Overrides
    AccountInfo fetchAccountInfo() override {
        if (config_.apiKey.empty()) throw std::runtime_error("API Key required");

        AccountInfo info;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PRIVATE_REQUEST, exchangeName_, "", "Get Account Info AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/info"}});
        setCredentials(request);

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            const auto& data = d["data"];
                            if(data.HasMember("email")) info.accountType = data["email"].GetString(); // Mapping email to type/name for info
                            if(data.HasMember("accountGroup")) info.makerCommission = data["accountGroup"].GetInt(); // Storing group
                            // Balances are in a separate endpoint usually, but Account Info gives permissions
                            if(data.HasMember("tradePermission")) info.canTrade = data["tradePermission"].GetBool();
                            if(data.HasMember("viewPermission")) info.canDeposit = data["viewPermission"].GetBool();
                            if(data.HasMember("transferPermission")) info.canWithdraw = data["transferPermission"].GetBool();
                        }
                    }
                }
            }
        }
        // Fetch balances separately as they are usually not in /info
        auto balances = fetchBalance();
        for(auto const& [asset, amount] : balances) {
            info.balances[asset] = amount;
        }
        return info;
    }

    std::map<std::string, double> fetchBalance() override {
        if (config_.apiKey.empty()) throw std::runtime_error("API Key required");

        std::map<std::string, double> balances;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PRIVATE_REQUEST, exchangeName_, "", "Get Balance AscendEX");
        request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/cash/balance"}});
        setCredentials(request);

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                        if(!d.HasParseError() && d.HasMember("data")) {
                            for(const auto& b : d["data"].GetArray()) {
                                std::string asset = b["asset"].GetString();
                                double free = std::stod(b["availableBalance"].GetString());
                                balances[asset] = free;
                            }
                        }
                    }
                }
            }
        }
        return balances;
    }

    std::vector<Trade> fetchMyTrades(const std::string& symbol, int limit = 100) override {
        if (config_.apiKey.empty()) throw std::runtime_error("API Key required");

        std::vector<Trade> trades;
        // Not implemented fully yet as it requires pagination logic or V2 endpoint
        return trades;
    }

    std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) override {
        if (config_.apiKey.empty()) throw std::runtime_error("API Key required");
        // Use standard CCAPI operation if possible, or Generic
        return GenericExchange::createOrder(symbol, side, amount, price);
    }

    std::string cancelOrder(const std::string& symbol, const std::string& orderId) override {
        if (config_.apiKey.empty()) throw std::runtime_error("API Key required");
        return GenericExchange::cancelOrder(symbol, orderId);
    }

private:
    void setCredentials(ccapi::Request& request) {
        std::map<std::string, std::string> creds;
        creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
        creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
        if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
        request.setCredential(creds);
    }
};

}
