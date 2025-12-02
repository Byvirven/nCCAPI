#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace unified_crypto {

// Data Structures
struct Ticker {
    std::string symbol;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double bidSize = 0.0;
    double askPrice = 0.0;
    double askSize = 0.0;
    std::string timestamp;
};

struct TickerStats {
    std::string symbol;
    double priceChange = 0.0;
    double priceChangePercent = 0.0;
    double weightedAvgPrice = 0.0;
    double prevClosePrice = 0.0;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double askPrice = 0.0;
    double openPrice = 0.0;
    double highPrice = 0.0;
    double lowPrice = 0.0;
    double volume = 0.0;
    double quoteVolume = 0.0;
    long long openTime = 0;
    long long closeTime = 0;
    long long tradeCount = 0;
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
    std::string symbol;
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
    std::string status;
    std::string type;
    double minSize = 0.0;
    double tickSize = 0.0;
    double stepSize = 0.0;
};

struct Order {
    std::string id;
    std::string symbol;
    std::string side;
    double price = 0.0;
    double size = 0.0;
    double filled = 0.0;
    std::string status;
    std::string clientOrderId;
    std::string timestamp;
};

struct BalanceUpdate {
    std::string asset;
    double free = 0.0;
    double locked = 0.0;
    std::string timestamp;
};

struct AccountInfo {
    int makerCommission = 0;
    int takerCommission = 0;
    int buyerCommission = 0;
    int sellerCommission = 0;
    bool canTrade = false;
    bool canWithdraw = false;
    bool canDeposit = false;
    long long updateTime = 0;
    std::string accountType;
    std::map<std::string, double> balances; // free + locked? or just free? Typically free. Or struct?
    // Let's use simple map for free balance here, or full structure.
    // Given fetchBalance exists, let's keep this focused on metadata + balances.
};

struct ExchangeConfig {
    std::string apiKey;
    std::string apiSecret;
    std::string passphrase;
};

struct TradingFees {
    double maker;
    double taker;
};

class Exchange {
public:
    virtual ~Exchange() = default;

    // Callbacks
    using TickerCallback = std::function<void(const Ticker&)>;
    using OrderBookCallback = std::function<void(const OrderBook&)>;
    using TradeCallback = std::function<void(const Trade&)>;
    using OHLCVCallback = std::function<void(const OHLCV&)>;
    using OrderUpdateCallback = std::function<void(const Order&)>;
    using AccountUpdateCallback = std::function<void(const BalanceUpdate&)>;

    virtual void setOnTicker(TickerCallback cb) = 0;
    virtual void setOnOrderBook(OrderBookCallback cb) = 0;
    virtual void setOnTrade(TradeCallback cb) = 0;
    virtual void setOnOHLCV(OHLCVCallback cb) = 0;
    virtual void setOnOrderUpdate(OrderUpdateCallback cb) = 0;
    virtual void setOnAccountUpdate(AccountUpdateCallback cb) = 0;

    // WebSocket - Public
    virtual void subscribeTicker(const std::string& symbol) = 0;
    virtual void subscribeOrderBook(const std::string& symbol, int depth = 10) = 0;
    virtual void subscribeTrades(const std::string& symbol) = 0;
    virtual void subscribeOHLCV(const std::string& symbol, const std::string& interval = "60") = 0;

    // WebSocket - Private
    virtual void subscribeOrderUpdates(const std::string& symbol) = 0;
    virtual void subscribeAccountUpdates() = 0;

    // REST - Public
    virtual Ticker fetchTicker(const std::string& symbol) = 0;
    virtual OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) = 0;
    virtual std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) = 0;
    virtual std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) = 0;
    virtual std::vector<Instrument> fetchInstruments() = 0;

    virtual Instrument fetchInstrument(const std::string& symbol) = 0;
    virtual std::vector<OHLCV> fetchOHLCVHistorical(const std::string& symbol, const std::string& timeframe, const std::string& startTime, const std::string& endTime, int limit = 1000) = 0;
    virtual std::vector<Trade> fetchTradesHistorical(const std::string& symbol, const std::string& startTime, const std::string& endTime, int limit = 1000) = 0;
    virtual std::string sendCustomRequest(const std::string& method, const std::string& path, const std::map<std::string, std::string>& params = {}) = 0;

    // New Public
    virtual TickerStats fetchTicker24h(const std::string& symbol) = 0;
    virtual long long fetchServerTime() = 0;

    // REST - Private
    virtual std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) = 0;
    virtual std::string cancelOrder(const std::string& symbol, const std::string& orderId) = 0;
    virtual std::vector<std::string> cancelAllOrders(const std::string& symbol) = 0;
    virtual Order fetchOrder(const std::string& symbol, const std::string& orderId) = 0;
    virtual std::vector<Order> fetchOpenOrders(const std::string& symbol) = 0;
    virtual std::vector<Trade> fetchMyTrades(const std::string& symbol, int limit = 100) = 0;
    virtual std::map<std::string, double> fetchBalance() = 0;
    virtual TradingFees fetchTradingFees(const std::string& symbol) = 0;

    // New Private
    virtual AccountInfo fetchAccountInfo() = 0;
};

}
