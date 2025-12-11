#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_EXCHANGE_BINANCE
#define CCAPI_ENABLE_EXCHANGE_BINANCE_US
#define CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_OKX
#define CCAPI_ENABLE_EXCHANGE_HUOBI
#define CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#define CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#define CCAPI_ENABLE_EXCHANGE_KUCOIN
#define CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BYBIT
#define CCAPI_ENABLE_EXCHANGE_COINBASE
#define CCAPI_ENABLE_EXCHANGE_KRAKEN
#define CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BITFINEX
#define CCAPI_ENABLE_EXCHANGE_BITMEX
#define CCAPI_ENABLE_EXCHANGE_GEMINI
#define CCAPI_ENABLE_EXCHANGE_DERIBIT
#define CCAPI_ENABLE_EXCHANGE_ASCENDEX
#define CCAPI_ENABLE_EXCHANGE_GATEIO
#define CCAPI_ENABLE_EXCHANGE_MEXC
#define CCAPI_ENABLE_EXCHANGE_BITSTAMP
#define CCAPI_ENABLE_EXCHANGE_BITGET

#include "ccapi_cpp/ccapi_session.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <ctime>
#include <rapidjson/document.h>

namespace ccapi { Logger* Logger::logger = nullptr; }

using namespace ccapi;

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================

struct Candle {
    int64_t timestamp = 0;
    std::string open, high, low, close, volume;
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Time: " << timestamp << ", O: " << open << ", H: " << high 
            << ", L: " << low << ", C: " << close << ", V: " << volume;
        return oss.str();
    }
    
    bool isValid() const {
        return timestamp > 0 && !open.empty() && !close.empty();
    }
};

struct InstrumentInfo {
    std::string symbol;
    std::string baseAsset;
    std::string quoteAsset;
    std::string status;
    bool isTradable = true;
    int liquidityScore = 0;
    
    std::string toString() const {
        return symbol + " [" + baseAsset + "/" + quoteAsset + "] Score: " + std::to_string(liquidityScore);
    }
};

struct ExchangeConfig {
    std::string klinePath;
    std::string symbolParam;
    std::string intervalParam;
    std::string intervalValue;
    std::string startParam;
    std::string endParam;
    std::string limitParam;
    bool timestampInMs;
    std::string dataPath;
    int tsIdx, oIdx, hIdx, lIdx, cIdx, vIdx;
    std::string instrumentType;
    std::string symbolPattern;
    bool useGenericInstruments;
    std::string instrumentsPath;
    std::string instrumentsQuery;
    std::string instrumentsDataPath;
};

// ============================================================================
// CONFIGURATION DES EXCHANGES
// ============================================================================

class ExchangeRegistry {
public:
    static const ExchangeConfig& getConfig(const std::string& exchange) {
        static std::map<std::string, ExchangeConfig> configs = {
            {"binance", {
                "/api/v3/klines", "symbol", "interval", "1m", "startTime", "endTime", "limit",
                true, "", 0, 1, 2, 3, 4, 5, "", 
                "^([A-Z0-9]+)(USDT|USDC|USD|BTC|ETH|BNB|EUR|BUSD|FDUSD|TRY)$",
                false, "", "", ""
            }},
            {"binance-us", {
                "/api/v3/klines", "symbol", "interval", "1m", "startTime", "endTime", "limit",
                true, "", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USD|USDT|USDC|BTC|ETH)$",
                true, "/api/v3/exchangeInfo", "", "symbols"
            }},
            {"binance-usds-futures", {
                "/fapi/v1/klines", "symbol", "interval", "1m", "startTime", "endTime", "limit",
                true, "", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USDT|USDC)$",
                false, "", "", ""
            }},
            {"binance-coin-futures", {
                "/dapi/v1/klines", "symbol", "interval", "1m", "startTime", "endTime", "limit",
                true, "", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USD)(_PERP|_\\d+)?$",
                false, "", "", ""
            }},
            {"okx", {
                "/api/v5/market/candles", "instId", "bar", "1m", "before", "after", "limit",
                true, "data", 0, 1, 2, 3, 4, 5, "SPOT",
                "^([A-Z0-9]+)-([A-Z0-9]+)$",
                false, "", "", ""
            }},
            {"bybit", {
                "/v5/market/kline", "symbol", "interval", "1", "start", "end", "limit",
                true, "result.list", 0, 1, 2, 3, 4, 5, "spot",
                "^([A-Z0-9]+)(USDT|USDC|BTC|ETH)$",
                false, "", "", ""
            }},
            {"kucoin", {
                "/api/v1/market/candles", "symbol", "type", "1min", "startAt", "endAt", "",
                false, "data", 0, 1, 3, 4, 2, 5, "",
                "^([A-Z0-9]+)-([A-Z0-9]+)$",
                false, "", "", ""
            }},
            {"kucoin-futures", {
                "/api/v1/kline/query", "symbol", "granularity", "1", "from", "to", "",
                true, "data", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USDTM|USDM)$",
                false, "", "", ""
            }},
            {"coinbase", {
                "/products/{symbol}/candles", "", "granularity", "60", "start", "end", "",
                false, "", 0, 3, 2, 1, 4, 5, "",
                "^([A-Z0-9]+)-([A-Z0-9]+)$",
                false, "", "", ""
            }},
            {"kraken", {
                "/0/public/OHLC", "pair", "interval", "1", "since", "", "",
                false, "result", 0, 1, 2, 3, 4, 6, "",
                "^([A-Z0-9]+)(USD|USDT|EUR|BTC|ETH)$",
                false, "", "", ""
            }},
            {"kraken-futures", {
                "/api/charts/v1/trade/{symbol}/1m", "", "", "", "from", "to", "",
                false, "candles", 0, 1, 2, 3, 4, 5, "",
                "^(PF|PI)_([A-Z]+)(USD)$",
                true, "/derivatives/api/v3/instruments", "", "instruments"
            }},
            {"bitfinex", {
                "/v2/candles/trade:1m:{symbol}/hist", "", "", "", "start", "end", "limit",
                true, "", 0, 1, 3, 4, 2, 5, "",
                "^t([A-Z0-9]+)(:)?([A-Z]{3,4})$",
                false, "", "", ""
            }},
            {"huobi", {
                "/market/history/kline", "symbol", "period", "1min", "", "", "size",
                false, "data", -1, -1, -1, -1, -1, -1, "",
                "^([a-z0-9]+)(usdt|usdc|btc|eth|husd)$",
                false, "", "", ""
            }},
            {"huobi-usdt-swap", {
                "/linear-swap-ex/market/history/kline", "contract_code", "period", "1min", "", "", "size",
                false, "data", -1, -1, -1, -1, -1, -1, "swap",
                "^([A-Z]+)(-USDT)?$",
                false, "", "", ""
            }},
            {"huobi-coin-swap", {
                "/swap-ex/market/history/kline", "contract_code", "period", "1min", "", "", "size",
                false, "data", -1, -1, -1, -1, -1, -1, "swap",
                "^([A-Z]+)(-USD)?$",
                false, "", "", ""
            }},
            {"gemini", {
                "/v2/candles/{symbol}/1m", "", "", "", "", "", "",
                true, "", 0, 1, 2, 3, 4, 5, "",
                "^([a-z0-9]+)(usd|btc|eth|eur|gbp|sgd)$",
                false, "", "", ""
            }},
            {"bitmex", {
                "/api/v1/trade/bucketed", "symbol", "binSize", "1m", "startTime", "endTime", "count",
                false, "", -1, -1, -1, -1, -1, -1, "",
                "^([A-Z]+)(USD|USDT)$",
                true, "/api/v1/instrument/active", "", ""
            }},
            {"deribit", {
                "/api/v2/public/get_tradingview_chart_data", "instrument_name", "resolution", "1", "start_timestamp", "end_timestamp", "",
                true, "result", -1, -1, -1, -1, -1, -1, "future",
                "^([A-Z]+)-(PERPETUAL|\\d+)$",
                false, "", "", ""
            }},
            {"mexc", {
                "/api/v3/klines", "symbol", "interval", "1m", "startTime", "endTime", "limit",
                true, "", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USDT|USDC|BTC|ETH)$",
                false, "", "", ""
            }},
            {"gateio", {
                "/api/v4/spot/candlesticks", "currency_pair", "interval", "1m", "from", "to", "limit",
                false, "", 0, 5, 3, 4, 2, 1, "",
                "^([A-Z0-9]+)_([A-Z0-9]+)$",
                false, "", "", ""
            }},
            {"ascendex", {
                "/api/pro/v1/barhist", "symbol", "interval", "1", "from", "to", "n",
                true, "data", -1, -1, -1, -1, -1, -1, "",
                "^([A-Z0-9]+)/([A-Z0-9]+)$",
                false, "", "", ""
            }},
            {"bitstamp", {
                "/api/v2/ohlc/{symbol}/", "", "step", "60", "", "", "limit",
                false, "data.ohlc", -1, -1, -1, -1, -1, -1, "",
                "^([a-z0-9]+)(usd|eur|btc|eth|gbp)$",
                false, "", "", ""
            }},
            {"bitget", {
                "/api/v2/spot/market/candles", "symbol", "granularity", "1min", "startTime", "endTime", "limit",
                true, "data", 0, 1, 2, 3, 4, 5, "",
                "^([A-Z0-9]+)(USDT|USDC|BTC|ETH)$",
                false, "", "", ""
            }}
        };
        return configs.at(exchange);
    }
    
    static std::vector<std::string> getAllExchanges() {
        return {"binance", "binance-us", "binance-usds-futures", "binance-coin-futures",
                "okx", "bybit", "kucoin", "kucoin-futures",
                "coinbase", "kraken", "kraken-futures", "bitfinex", "huobi",
                "huobi-usdt-swap", "huobi-coin-swap", "gemini", "bitmex", "deribit",
                "mexc", "gateio", "ascendex", "bitstamp", "bitget"};
    }
};

// ============================================================================
// UTILITAIRES
// ============================================================================

class Utils {
public:
    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }
    
    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }
    
    static std::pair<std::string, std::string> parseSymbol(const std::string& symbol, const std::string& pattern) {
        if (pattern.empty()) return {"", ""};
        try {
            std::regex re(pattern, std::regex::icase);
            std::smatch match;
            if (std::regex_match(symbol, match, re) && match.size() >= 3) {
                return {toUpper(match[1].str()), toUpper(match[2].str())};
            }
        } catch (...) {}
        return {"", ""};
    }
    
    static int calculateLiquidityScore(const std::string& base, const std::string& quote) {
        static const std::map<std::string, int> baseScores = {
            {"BTC", 100}, {"ETH", 90}, {"SOL", 70}, {"BNB", 70}, {"XRP", 70},
            {"DOGE", 60}, {"ADA", 60}, {"AVAX", 60}, {"DOT", 55}, {"LINK", 55},
            {"MATIC", 50}, {"LTC", 50}, {"UNI", 45}, {"ATOM", 45}, {"ARB", 45}
        };
        static const std::map<std::string, int> quoteScores = {
            {"USDT", 50}, {"USD", 45}, {"USDC", 40}, {"BUSD", 35}, {"FDUSD", 35},
            {"BTC", 30}, {"ETH", 25}, {"EUR", 20}, {"USDTM", 40}, {"USDM", 35}
        };
        
        int score = 10;
        auto bit = baseScores.find(base);
        if (bit != baseScores.end()) score += bit->second;
        auto qit = quoteScores.find(quote);
        if (qit != quoteScores.end()) score += qit->second;
        return score;
    }
    
    static std::string getJsonValue(const rapidjson::Value& val) {
        if (val.IsString()) return val.GetString();
        if (val.IsInt64()) return std::to_string(val.GetInt64());
        if (val.IsUint64()) return std::to_string(val.GetUint64());
        if (val.IsDouble()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(8) << val.GetDouble();
            std::string s = oss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        }
        if (val.IsInt()) return std::to_string(val.GetInt());
        return "";
    }
    
    static const rapidjson::Value* navigateJson(const rapidjson::Value& doc, const std::string& path) {
        if (path.empty()) return &doc;
        
        const rapidjson::Value* current = &doc;
        std::istringstream iss(path);
        std::string token;
        
        while (std::getline(iss, token, '.')) {
            if (current->IsObject() && current->HasMember(token.c_str())) {
                current = &(*current)[token.c_str()];
            } else {
                return nullptr;
            }
        }
        return current;
    }
    
    static std::string transformSymbolForCandles(const std::string& exchange, const std::string& symbol) {
        if (exchange == "huobi-usdt-swap") {
            if (symbol.find("-USDT") == std::string::npos) return symbol + "-USDT";
        } else if (exchange == "huobi-coin-swap") {
            if (symbol.find("-USD") == std::string::npos) return symbol + "-USD";
        } else if (exchange == "bitfinex") {
            if (!symbol.empty() && symbol[0] != 't') return "t" + symbol;
        }
        return symbol;
    }
    
    static std::string toIso8601(int64_t timestamp) {
        std::time_t t = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::gmtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
    
    static int64_t fromIso8601(const std::string& iso) {
        std::tm tm = {};
        std::istringstream ss(iso);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return static_cast<int64_t>(timegm(&tm));
    }
};

// ============================================================================
// PARSER DE CANDLESTICKS
// ============================================================================

class CandleParser {
public:
    static std::vector<Candle> parse(const std::string& exchange, const std::string& json, int limit = 60) {
        std::vector<Candle> candles;
        
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        if (doc.HasParseError()) {
            std::cerr << "[ERROR] JSON parse error" << std::endl;
            return candles;
        }
        
        if (doc.IsObject()) {
            if ((doc.HasMember("code") && doc["code"].IsInt() && doc["code"].GetInt() != 0) ||
                doc.HasMember("error") || 
                (doc.HasMember("retCode") && doc["retCode"].GetInt() != 0)) {
                std::cerr << "[ERROR] API error: " << json.substr(0, 500) << std::endl;
                return candles;
            }
        }
        
        const auto& config = ExchangeRegistry::getConfig(exchange);
        const rapidjson::Value* data = Utils::navigateJson(doc, config.dataPath);
        
        if (!data) {
            if (doc.IsArray()) {
                data = &doc;
            } else {
                std::cerr << "[ERROR] Cannot navigate to: " << config.dataPath << std::endl;
                return candles;
            }
        }
        
        if (exchange == "huobi" || exchange == "huobi-usdt-swap" || exchange == "huobi-coin-swap") {
            candles = parseHuobi(*data);
        } else if (exchange == "ascendex") {
            candles = parseAscendex(*data);
        } else if (exchange == "bitstamp") {
            candles = parseBitstamp(*data);
        } else if (exchange == "coinbase") {
            candles = parseCoinbase(*data);
        } else if (exchange == "bitmex") {
            candles = parseBitmex(*data);
        } else if (exchange == "deribit") {
            candles = parseDeribit(*data);
        } else if (exchange == "kraken") {
            candles = parseKraken(doc);
        } else if (exchange == "kraken-futures") {
            candles = parseKrakenFutures(*data);
        } else if (exchange == "bybit") {
            candles = parseBybit(*data);
        } else if (exchange == "okx") {
            candles = parseOkx(*data);
        } else {
            candles = parseGenericArray(*data, config);
        }
        
        std::sort(candles.begin(), candles.end(),
            [](const Candle& a, const Candle& b) { return a.timestamp < b.timestamp; });
        
        if (candles.size() > static_cast<size_t>(limit)) {
            candles = std::vector<Candle>(candles.end() - limit, candles.end());
        }
        
        return candles;
    }
    
private:
    static std::vector<Candle> parseGenericArray(const rapidjson::Value& data, const ExchangeConfig& config) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsArray() || item.Size() < 6) continue;
            
            Candle c;
            c.timestamp = std::stoll(Utils::getJsonValue(item[config.tsIdx]));
            if (config.timestampInMs) c.timestamp /= 1000;
            c.open = Utils::getJsonValue(item[config.oIdx]);
            c.high = Utils::getJsonValue(item[config.hIdx]);
            c.low = Utils::getJsonValue(item[config.lIdx]);
            c.close = Utils::getJsonValue(item[config.cIdx]);
            c.volume = Utils::getJsonValue(item[config.vIdx]);
            
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseHuobi(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsObject()) continue;
            Candle c;
            c.timestamp = item.HasMember("id") ? item["id"].GetInt64() : 0;
            c.open = item.HasMember("open") ? Utils::getJsonValue(item["open"]) : "";
            c.high = item.HasMember("high") ? Utils::getJsonValue(item["high"]) : "";
            c.low = item.HasMember("low") ? Utils::getJsonValue(item["low"]) : "";
            c.close = item.HasMember("close") ? Utils::getJsonValue(item["close"]) : "";
            c.volume = item.HasMember("vol") ? Utils::getJsonValue(item["vol"]) : "0";
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseAscendex(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& bar : data.GetArray()) {
            if (!bar.HasMember("data")) continue;
            const auto& d = bar["data"];
            Candle c;
            c.timestamp = d["ts"].GetInt64() / 1000;
            c.open = d["o"].GetString();
            c.high = d["h"].GetString();
            c.low = d["l"].GetString();
            c.close = d["c"].GetString();
            c.volume = d["v"].GetString();
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseBitstamp(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsObject()) continue;
            Candle c;
            c.timestamp = std::stoll(item["timestamp"].GetString());
            c.open = item["open"].GetString();
            c.high = item["high"].GetString();
            c.low = item["low"].GetString();
            c.close = item["close"].GetString();
            c.volume = item["volume"].GetString();
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseCoinbase(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsArray() || item.Size() < 6) continue;
            Candle c;
            // Format: [time, low, high, open, close, volume]
            c.timestamp = item[0].GetInt64();
            c.low = Utils::getJsonValue(item[1]);
            c.high = Utils::getJsonValue(item[2]);
            c.open = Utils::getJsonValue(item[3]);
            c.close = Utils::getJsonValue(item[4]);
            c.volume = Utils::getJsonValue(item[5]);
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseBitmex(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsObject()) continue;
            Candle c;
            
            if (item.HasMember("timestamp")) {
                std::string ts = item["timestamp"].GetString();
                c.timestamp = Utils::fromIso8601(ts);
            }
            
            c.open = item.HasMember("open") ? Utils::getJsonValue(item["open"]) : "";
            c.high = item.HasMember("high") ? Utils::getJsonValue(item["high"]) : "";
            c.low = item.HasMember("low") ? Utils::getJsonValue(item["low"]) : "";
            c.close = item.HasMember("close") ? Utils::getJsonValue(item["close"]) : "";
            c.volume = item.HasMember("volume") ? Utils::getJsonValue(item["volume"]) : "0";
            
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseDeribit(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsObject()) return candles;
        
        if (data.HasMember("ticks") && data["ticks"].IsArray()) {
            const auto& ticks = data["ticks"].GetArray();
            const auto& opens = data.HasMember("open") ? data["open"].GetArray() : ticks;
            const auto& highs = data.HasMember("high") ? data["high"].GetArray() : ticks;
            const auto& lows = data.HasMember("low") ? data["low"].GetArray() : ticks;
            const auto& closes = data.HasMember("close") ? data["close"].GetArray() : ticks;
            const auto& volumes = data.HasMember("volume") ? data["volume"].GetArray() : ticks;
            
            for (size_t i = 0; i < ticks.Size() && i < opens.Size(); ++i) {
                Candle c;
                c.timestamp = ticks[i].GetInt64() / 1000;
                c.open = Utils::getJsonValue(opens[i]);
                c.high = Utils::getJsonValue(highs[i]);
                c.low = Utils::getJsonValue(lows[i]);
                c.close = Utils::getJsonValue(closes[i]);
                c.volume = Utils::getJsonValue(volumes[i]);
                if (c.isValid()) candles.push_back(c);
            }
        }
        return candles;
    }
    
    static std::vector<Candle> parseKraken(const rapidjson::Document& doc) {
        std::vector<Candle> candles;
        if (!doc.HasMember("result")) return candles;
        
        const auto& result = doc["result"];
        for (auto it = result.MemberBegin(); it != result.MemberEnd(); ++it) {
            if (std::string(it->name.GetString()) == "last") continue;
            if (!it->value.IsArray()) continue;
            
            for (const auto& item : it->value.GetArray()) {
                if (!item.IsArray() || item.Size() < 7) continue;
                Candle c;
                c.timestamp = item[0].GetInt64();
                c.open = Utils::getJsonValue(item[1]);
                c.high = Utils::getJsonValue(item[2]);
                c.low = Utils::getJsonValue(item[3]);
                c.close = Utils::getJsonValue(item[4]);
                c.volume = Utils::getJsonValue(item[6]);
                if (c.isValid()) candles.push_back(c);
            }
            break;
        }
        return candles;
    }
    
    static std::vector<Candle> parseKrakenFutures(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsObject()) continue;
            Candle c;
            c.timestamp = item.HasMember("time") ? item["time"].GetInt64() / 1000 : 0;
            c.open = Utils::getJsonValue(item["open"]);
            c.high = Utils::getJsonValue(item["high"]);
            c.low = Utils::getJsonValue(item["low"]);
            c.close = Utils::getJsonValue(item["close"]);
            c.volume = item.HasMember("volume") ? Utils::getJsonValue(item["volume"]) : "0";
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseBybit(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsArray() || item.Size() < 6) continue;
            Candle c;
            c.timestamp = std::stoll(Utils::getJsonValue(item[0])) / 1000;
            c.open = Utils::getJsonValue(item[1]);
            c.high = Utils::getJsonValue(item[2]);
            c.low = Utils::getJsonValue(item[3]);
            c.close = Utils::getJsonValue(item[4]);
            c.volume = Utils::getJsonValue(item[5]);
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
    
    static std::vector<Candle> parseOkx(const rapidjson::Value& data) {
        std::vector<Candle> candles;
        if (!data.IsArray()) return candles;
        
        for (const auto& item : data.GetArray()) {
            if (!item.IsArray() || item.Size() < 6) continue;
            Candle c;
            c.timestamp = std::stoll(Utils::getJsonValue(item[0])) / 1000;
            c.open = Utils::getJsonValue(item[1]);
            c.high = Utils::getJsonValue(item[2]);
            c.low = Utils::getJsonValue(item[3]);
            c.close = Utils::getJsonValue(item[4]);
            c.volume = Utils::getJsonValue(item[5]);
            if (c.isValid()) candles.push_back(c);
        }
        return candles;
    }
};

// ============================================================================
// PARSER D'INSTRUMENTS GENERIQUE
// ============================================================================

class GenericInstrumentParser {
public:
    static std::vector<InstrumentInfo> parse(const std::string& exchange, const std::string& json) {
        std::vector<InstrumentInfo> instruments;
        
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        if (doc.HasParseError()) {
            std::cerr << "[ERROR] JSON parse error for instruments" << std::endl;
            return instruments;
        }
        
        // Check for API errors
        if (doc.IsObject()) {
            if (doc.HasMember("retCode") && doc["retCode"].GetInt() != 0) {
                std::cerr << "[ERROR] API error: " << json.substr(0, 500) << std::endl;
                return instruments;
            }
        }
        
        const auto& config = ExchangeRegistry::getConfig(exchange);
        const rapidjson::Value* data = Utils::navigateJson(doc, config.instrumentsDataPath);
        
        if (!data || !data->IsArray()) {
            if (doc.IsArray()) {
                data = &doc;
            } else {
                std::cerr << "[ERROR] Cannot find instruments data at: " << config.instrumentsDataPath << std::endl;
                std::cerr << "[DEBUG] Keys available: ";
                if (doc.IsObject()) {
                    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
                        std::cerr << it->name.GetString() << " ";
                    }
                }
                std::cerr << std::endl;
                return instruments;
            }
        }
        
        std::cout << "[DEBUG] Found " << data->GetArray().Size() << " raw instruments" << std::endl;
        
        for (const auto& item : data->GetArray()) {
            if (!item.IsObject()) continue;
            
            InstrumentInfo info;
            
            if (exchange == "binance-us") {
                info.symbol = item.HasMember("symbol") ? item["symbol"].GetString() : "";
                info.baseAsset = item.HasMember("baseAsset") ? item["baseAsset"].GetString() : "";
                info.quoteAsset = item.HasMember("quoteAsset") ? item["quoteAsset"].GetString() : "";
                info.status = item.HasMember("status") ? item["status"].GetString() : "";
                info.isTradable = (info.status == "TRADING");
            } else if (exchange == "kraken-futures") {
                info.symbol = item.HasMember("symbol") ? item["symbol"].GetString() : "";
                info.isTradable = item.HasMember("tradeable") ? item["tradeable"].GetBool() : true;
                auto parsed = Utils::parseSymbol(info.symbol, config.symbolPattern);
                info.baseAsset = parsed.first;
                info.quoteAsset = parsed.second;
            } else if (exchange == "bitmex") {
                info.symbol = item.HasMember("symbol") ? item["symbol"].GetString() : "";
                info.baseAsset = item.HasMember("rootSymbol") ? item["rootSymbol"].GetString() : "";
                info.quoteAsset = item.HasMember("quoteCurrency") ? item["quoteCurrency"].GetString() : "";
                std::string state = item.HasMember("state") ? item["state"].GetString() : "";
                info.isTradable = (state == "Open");
            }
            
            if (!info.symbol.empty()) {
                info.baseAsset = Utils::toUpper(info.baseAsset);
                info.quoteAsset = Utils::toUpper(info.quoteAsset);
                
                if (info.baseAsset.empty() || info.quoteAsset.empty()) {
                    auto [base, quote] = Utils::parseSymbol(info.symbol, config.symbolPattern);
                    if (!base.empty()) info.baseAsset = base;
                    if (!quote.empty()) info.quoteAsset = quote;
                }
                
                info.liquidityScore = Utils::calculateLiquidityScore(info.baseAsset, info.quoteAsset);
                
                // Lower threshold for derivatives
                int minScore = (exchange == "kraken-futures") ? 40 : 50;
                
                if (info.isTradable && info.liquidityScore >= minScore) {
                    instruments.push_back(info);
                }
            }
        }
        
        return instruments;
    }
};

// ============================================================================
// GESTIONNAIRE D'ÉTAT
// ============================================================================

struct AppState {
    std::string exchange;
    std::vector<InstrumentInfo> instruments;
    InstrumentInfo selectedInstrument;
    std::vector<Candle> candles;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool instrumentsReceived = false;
    bool candlesReceived = false;
    bool hasError = false;
    std::string errorMessage;
    
    void reset() {
        instruments.clear();
        candles.clear();
        instrumentsReceived = candlesReceived = hasError = false;
        errorMessage.clear();
    }
    
    void setError(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        hasError = true;
        errorMessage = msg;
        candlesReceived = true;
        cv.notify_all();
    }
    
    void signalCandlesReceived() {
        std::lock_guard<std::mutex> lock(mtx);
        candlesReceived = true;
        cv.notify_all();
    }
};

AppState g_state;

// ============================================================================
// URL BUILDER
// ============================================================================

class UrlBuilder {
public:
    static std::pair<std::string, std::string> buildCandleRequest(
        const std::string& exchange, const std::string& rawSymbol, int64_t startSec, int64_t endSec) {
        
        const auto& config = ExchangeRegistry::getConfig(exchange);
        std::string symbol = Utils::transformSymbolForCandles(exchange, rawSymbol);
        std::string path = config.klinePath;
        std::ostringstream query;
        
        int64_t startTs = config.timestampInMs ? startSec * 1000 : startSec;
        int64_t endTs = config.timestampInMs ? endSec * 1000 : endSec;
        
        // Handle path templates
        size_t pos;
        while ((pos = path.find("{symbol}")) != std::string::npos) {
            std::string sym = (exchange == "bitstamp") ? Utils::toLower(symbol) : symbol;
            path.replace(pos, 8, sym);
        }
        
        if (exchange == "gemini") {
            // No query params
        } else if (exchange == "bitfinex") {
            query << "limit=60";
            query << "&" << config.startParam << "=" << startTs;
            query << "&" << config.endParam << "=" << endTs;
        } else if (exchange == "huobi" || exchange == "huobi-usdt-swap" || exchange == "huobi-coin-swap") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&size=60";
        } else if (exchange == "coinbase") {
            // Coinbase uses Unix timestamps in seconds
            query << "granularity=60";
            query << "&start=" << startSec;
            query << "&end=" << endSec;
        } else if (exchange == "kraken") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&since=" << startSec;
        } else if (exchange == "kraken-futures") {
            query << "from=" << startTs << "&to=" << endTs;
        } else if (exchange == "bitstamp") {
            query << config.intervalParam << "=" << config.intervalValue;
            query << "&limit=60";
        } else if (exchange == "okx") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&limit=60";
        } else if (exchange == "bybit") {
            query << "category=" << config.instrumentType;
            query << "&" << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&" << config.startParam << "=" << startTs;
            query << "&" << config.endParam << "=" << endTs;
            query << "&limit=60";
        } else if (exchange == "deribit") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&" << config.startParam << "=" << startTs;
            query << "&" << config.endParam << "=" << endTs;
        } else if (exchange == "bitmex") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&startTime=" << Utils::toIso8601(startSec);
            query << "&endTime=" << Utils::toIso8601(endSec);
            query << "&count=60";
        } else if (exchange == "kucoin-futures") {
            query << config.symbolParam << "=" << symbol;
            query << "&" << config.intervalParam << "=" << config.intervalValue;
            query << "&" << config.startParam << "=" << startTs;
            query << "&" << config.endParam << "=" << endTs;
        } else {
            if (!config.symbolParam.empty()) query << config.symbolParam << "=" << symbol;
            if (!config.intervalParam.empty()) query << "&" << config.intervalParam << "=" << config.intervalValue;
            if (!config.startParam.empty()) query << "&" << config.startParam << "=" << startTs;
            if (!config.endParam.empty()) query << "&" << config.endParam << "=" << endTs;
            if (!config.limitParam.empty()) query << "&" << config.limitParam << "=60";
        }
        
        std::string queryStr = query.str();
        if (!queryStr.empty() && queryStr[0] == '&') queryStr = queryStr.substr(1);
        
        return {path, queryStr};
    }
};

// ============================================================================
// EVENT HANDLER
// ============================================================================

class MultiExchangeHandler : public EventHandler {
    bool candleRequestSent = false;
    bool candleResponseReceived = false;
    
public:
    void reset() {
        candleRequestSent = false;
        candleResponseReceived = false;
    }
    
    void processEvent(const Event& event, Session* session) override {
        if (event.getType() == Event::Type::RESPONSE) {
            for (const auto& message : event.getMessageList()) {
                std::string corrId = message.getCorrelationIdList().empty() ? "" : message.getCorrelationIdList().front();
                
                if (message.getType() == Message::Type::RESPONSE_ERROR) {
                    handleError(message, corrId);
                    return;
                }
                
                if (corrId == "get_instruments" && message.getType() == Message::Type::GET_INSTRUMENTS) {
                    handleInstruments(message, session);
                } else if (corrId == "get_instruments_generic") {
                    handleGenericInstruments(message, session);
                } else if (corrId == "get_candles" && !candleResponseReceived) {
                    candleResponseReceived = true;
                    handleCandles(message);
                }
            }
        }
    }
    
private:
    void handleError(const Message& message, const std::string& corrId) {
        std::ostringstream oss;
        for (const auto& elem : message.getElementList()) {
            for (const auto& [k, v] : elem.getNameValueMap()) {
                oss << k << "=" << v << " ";
            }
        }
        std::cerr << "[ERROR] " << corrId << ": " << oss.str() << std::endl;
        g_state.setError(oss.str());
    }
    
    void handleInstruments(const Message& message, Session* session) {
        std::cout << "[INFO] Processing instruments..." << std::endl;
        
        const auto& config = ExchangeRegistry::getConfig(g_state.exchange);
        std::vector<InstrumentInfo> instruments;
        
        for (const auto& element : message.getElementList()) {
            InstrumentInfo info;
            const auto& nvMap = element.getNameValueMap();
            
            auto get = [&nvMap](const std::string& key) -> std::string {
                auto it = nvMap.find(key);
                return it != nvMap.end() ? it->second : "";
            };
            
            info.symbol = get("INSTRUMENT");
            if (info.symbol.empty()) continue;
            
            info.baseAsset = get("BASE_ASSET");
            info.quoteAsset = get("QUOTE_ASSET");
            info.status = get("INSTRUMENT_STATUS");
            
            if (info.baseAsset.empty() || info.quoteAsset.empty()) {
                auto [base, quote] = Utils::parseSymbol(info.symbol, config.symbolPattern);
                if (!base.empty()) info.baseAsset = base;
                if (!quote.empty()) info.quoteAsset = quote;
            }
            
            info.baseAsset = Utils::toUpper(info.baseAsset);
            info.quoteAsset = Utils::toUpper(info.quoteAsset);
            info.liquidityScore = Utils::calculateLiquidityScore(info.baseAsset, info.quoteAsset);
            
            if (info.liquidityScore >= 50) {
                instruments.push_back(info);
            }
        }
        
        processAndSelectInstrument(instruments, session);
    }
    
    void handleGenericInstruments(const Message& message, Session* session) {
        std::cout << "[INFO] Processing generic instruments..." << std::endl;
        
        std::string httpBody;
        std::string httpStatus = "200";
        
        for (const auto& element : message.getElementList()) {
            auto bodyIt = element.getNameValueMap().find("HTTP_BODY");
            if (bodyIt != element.getNameValueMap().end()) {
                httpBody = bodyIt->second;
            }
            auto statusIt = element.getNameValueMap().find("HTTP_STATUS_CODE");
            if (statusIt != element.getNameValueMap().end()) {
                httpStatus = statusIt->second;
            }
        }
        
        std::cout << "[DEBUG] HTTP Status: " << httpStatus << std::endl;
        
        if (httpStatus != "200") {
            std::cerr << "[ERROR] HTTP error: " << httpStatus << std::endl;
            std::cerr << "[DEBUG] Body: " << httpBody.substr(0, 500) << std::endl;
            g_state.setError("HTTP error: " + httpStatus);
            return;
        }
        
        if (httpBody.empty()) {
            std::cerr << "[ERROR] Empty response body" << std::endl;
            g_state.setError("Empty response");
            return;
        }
        
        std::cout << "[DEBUG] Response length: " << httpBody.length() << std::endl;
        
        std::vector<InstrumentInfo> instruments = GenericInstrumentParser::parse(g_state.exchange, httpBody);
        processAndSelectInstrument(instruments, session);
    }
    
    void processAndSelectInstrument(std::vector<InstrumentInfo>& instruments, Session* session) {
        if (instruments.empty()) {
            std::cerr << "[WARN] No suitable instruments found" << std::endl;
            g_state.setError("No suitable instruments");
            return;
        }
        
        std::sort(instruments.begin(), instruments.end(),
            [](const auto& a, const auto& b) { return a.liquidityScore > b.liquidityScore; });
        
        std::cout << "[INFO] Found " << instruments.size() << " instruments" << std::endl;
        std::cout << "[DEBUG] Top 5:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), instruments.size()); ++i) {
            std::cout << "  " << (i+1) << ". " << instruments[i].toString() << std::endl;
        }
        
        g_state.instruments = instruments;
        
        // Select from top 3 for highest liquidity
        size_t poolSize = std::min(size_t(3), instruments.size());
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(0, poolSize - 1);
        g_state.selectedInstrument = instruments[distr(gen)];
        
        std::cout << "[INFO] Selected: " << g_state.selectedInstrument.toString() << std::endl;
        
        if (!candleRequestSent) {
            candleRequestSent = true;
            requestCandles(session);
        }
    }
    
    void requestCandles(Session* session) {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t start = now - 3660;
        
        const std::string& symbol = g_state.selectedInstrument.symbol;
        auto [path, query] = UrlBuilder::buildCandleRequest(g_state.exchange, symbol, start, now);
        
        std::cout << "[INFO] Requesting candles via GENERIC_PUBLIC_REQUEST" << std::endl;
        std::cout << "[DEBUG] Path: " << path << std::endl;
        std::cout << "[DEBUG] Query: " << query << std::endl;
        
        Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, g_state.exchange, symbol, "get_candles");
        request.appendParam({
            {"HTTP_METHOD", "GET"},
            {"HTTP_PATH", path},
            {"HTTP_QUERY_STRING", query}
        });
        session->sendRequest(request);
    }
    
    void handleCandles(const Message& message) {
        std::string httpBody;
        std::string httpStatus;
        
        for (const auto& element : message.getElementList()) {
            auto bodyIt = element.getNameValueMap().find("HTTP_BODY");
            if (bodyIt != element.getNameValueMap().end()) {
                httpBody = bodyIt->second;
            }
            auto statusIt = element.getNameValueMap().find("HTTP_STATUS_CODE");
            if (statusIt != element.getNameValueMap().end()) {
                httpStatus = statusIt->second;
            }
        }
        
        if (!httpStatus.empty() && httpStatus != "200") {
            std::cerr << "[ERROR] HTTP status: " << httpStatus << std::endl;
            std::cerr << "[DEBUG] Response: " << httpBody.substr(0, 500) << std::endl;
        }
        
        if (!httpBody.empty()) {
            g_state.candles = CandleParser::parse(g_state.exchange, httpBody, 60);
        }
        
        std::cout << "[INFO] Received " << g_state.candles.size() << " candles" << std::endl;
        
        if (!g_state.candles.empty()) {
            std::cout << "[RESULT] First: " << g_state.candles.front().toString() << std::endl;
            std::cout << "[RESULT] Last:  " << g_state.candles.back().toString() << std::endl;
        }
        
        g_state.signalCandlesReceived();
    }
};

MultiExchangeHandler g_handler;

// ============================================================================
// REQUÊTES
// ============================================================================

void sendInstrumentsRequest(Session& session, const std::string& exchange) {
    const auto& config = ExchangeRegistry::getConfig(exchange);
    
    if (config.useGenericInstruments) {
        std::cout << "[INFO] Using GENERIC_PUBLIC_REQUEST for instruments" << std::endl;
        std::cout << "[DEBUG] Path: " << config.instrumentsPath << std::endl;
        std::cout << "[DEBUG] Query: " << config.instrumentsQuery << std::endl;
        
        Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, exchange, "", "get_instruments_generic");
        request.appendParam({
            {"HTTP_METHOD", "GET"},
            {"HTTP_PATH", config.instrumentsPath},
            {"HTTP_QUERY_STRING", config.instrumentsQuery}
        });
        session.sendRequest(request);
    } else {
        Request request(Request::Operation::GET_INSTRUMENTS, exchange, "", "get_instruments");
        
        if (exchange == "okx") {
            request.appendParam({{"INSTRUMENT_TYPE", "SPOT"}});
        } else if (exchange == "bybit") {
            request.appendParam({{"INSTRUMENT_TYPE", "spot"}});
        } else if (exchange == "huobi-usdt-swap" || exchange == "huobi-coin-swap") {
            request.appendParam({{"INSTRUMENT_TYPE", "swap"}});
        } else if (exchange == "deribit") {
            request.appendParam({{"INSTRUMENT_TYPE", "future"}, {"UNDERLYING", "BTC"}});
        }
        
        session.sendRequest(request);
    }
}

// ============================================================================
// MAIN
// ============================================================================

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <exchange>\n\nSupported exchanges:\n";
    for (const auto& ex : ExchangeRegistry::getAllExchanges()) {
        std::cout << "  " << ex << "\n";
    }
    std::cout << "\nNote: crypto.com is NOT supported by CCAPI\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string exchange = argv[1];
    
    try {
        ExchangeRegistry::getConfig(exchange);
    } catch (...) {
        std::cerr << "Unknown exchange: " << exchange << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "[START] Exchange: " << exchange << std::endl;
    std::cout << "========================================" << std::endl;
    
    g_state.exchange = exchange;
    g_state.reset();
    g_handler.reset();
    
    SessionOptions opts;
    opts.httpRequestTimeoutMilliseconds = 30000;
    
    SessionConfigs configs;
    Session session(opts, configs, &g_handler);
    
    std::cout << "[INFO] Fetching instruments..." << std::endl;
    sendInstrumentsRequest(session, exchange);
    
    {
        std::unique_lock<std::mutex> lock(g_state.mtx);
        g_state.cv.wait_for(lock, std::chrono::seconds(60), [] {
            return g_state.candlesReceived || g_state.hasError;
        });
    }
    
    std::string status;
    if (g_state.hasError) {
        status = "FAILED";
    } else if (g_state.candles.empty()) {
        status = "NO DATA";
    } else if (g_state.candles.size() < 55) {
        status = "PARTIAL (" + std::to_string(g_state.candles.size()) + "/60)";
    } else {
        status = "SUCCESS (" + std::to_string(g_state.candles.size()) + "/60)";
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "[SUMMARY]" << std::endl;
    std::cout << "  Exchange:       " << exchange << std::endl;
    std::cout << "  Instruments:    " << g_state.instruments.size() << std::endl;
    std::cout << "  Selected:       " << g_state.selectedInstrument.symbol << std::endl;
    std::cout << "  Candles:        " << g_state.candles.size() << std::endl;
    std::cout << "  Status:         " << status << std::endl;
    std::cout << "========================================" << std::endl;
    
    session.stop();
    
    // Success if we have at least 30 candles (50%)
    return (g_state.hasError || g_state.candles.size() < 30) ? 1 : 0;
}
