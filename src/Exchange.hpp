#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace unified_crypto {

// Data Structures (reused)
struct Ticker {
    std::string symbol;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double bidSize = 0.0;
    double askPrice = 0.0;
    double askSize = 0.0;
    std::string timestamp;
};

struct OrderBookEntry {
    double price;
    double size;
};

struct OrderBook {
    std::string symbol;
    std::vector<OrderBookEntry> bids;
    std::vector<OrderBookEntry> asks;
    std::string timestamp;
};

struct Trade {
    std::string id;
    std::string timestamp;
    std::string symbol;
    double price;
    double size;
    std::string side;
    bool isBuyerMaker;
};

struct OHLCV {
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

struct Instrument {
    std::string symbol;
    std::string baseAsset;
    std::string quoteAsset;
    // Extended Metadata
    std::string status;
    double minSize = 0.0;
    double tickSize = 0.0;
    double stepSize = 0.0;
};

struct ExchangeConfig {
    std::string apiKey;
    std::string apiSecret;
    std::string passphrase;
};

class Exchange {
public:
    virtual ~Exchange() = default;

    // Callbacks
    using TickerCallback = std::function<void(const Ticker&)>;
    using OrderBookCallback = std::function<void(const OrderBook&)>;
    using TradeCallback = std::function<void(const Trade&)>;
    using OHLCVCallback = std::function<void(const OHLCV&)>;

    virtual void setOnTicker(TickerCallback cb) = 0;
    virtual void setOnOrderBook(OrderBookCallback cb) = 0;
    virtual void setOnTrade(TradeCallback cb) = 0;
    virtual void setOnOHLCV(OHLCVCallback cb) = 0;

    // WebSocket
    virtual void subscribeTicker(const std::string& symbol) = 0;
    virtual void subscribeOrderBook(const std::string& symbol, int depth = 10) = 0;
    virtual void subscribeTrades(const std::string& symbol) = 0;
    virtual void subscribeOHLCV(const std::string& symbol, const std::string& interval = "60") = 0;

    // REST - Public
    virtual Ticker fetchTicker(const std::string& symbol) = 0;
    virtual OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) = 0;
    virtual std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) = 0;
    virtual std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) = 0;
    virtual std::vector<Instrument> fetchInstruments() = 0;

    // New REST - Public

    /**
     * @brief Fetch detailed information about a single instrument.
     */
    virtual Instrument fetchInstrument(const std::string& symbol) = 0;

    /**
     * @brief Fetch historical OHLCV candles within a time range.
     * @param startTime ISO 8601 string (e.g. "2024-01-01T00:00:00Z")
     * @param endTime ISO 8601 string
     */
    virtual std::vector<OHLCV> fetchOHLCVHistorical(const std::string& symbol, const std::string& timeframe, const std::string& startTime, const std::string& endTime, int limit = 1000) = 0;

    /**
     * @brief Fetch historical trades within a time range.
     * @param startTime ISO 8601 string
     * @param endTime ISO 8601 string
     */
    virtual std::vector<Trade> fetchTradesHistorical(const std::string& symbol, const std::string& startTime, const std::string& endTime, int limit = 1000) = 0;

    /**
     * @brief Send a generic public request to the exchange (useful for specific endpoints).
     * @param method HTTP method (GET, POST, etc.)
     * @param path API path (e.g. "/api/v3/ping")
     * @param params Query parameters or body
     * @return Raw response body as string
     */
    virtual std::string sendCustomRequest(const std::string& method, const std::string& path, const std::map<std::string, std::string>& params = {}) = 0;

    // Private
    virtual std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) = 0;
    virtual std::map<std::string, double> fetchBalance() = 0;
};

}
