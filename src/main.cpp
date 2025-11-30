#include "UnifiedExchange.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace unified_crypto;

void testExchange(const std::string& exchangeName, const std::string& symbol) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "Testing Exchange: " << exchangeName << " (" << symbol << ")" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        UnifiedExchange exchange(exchangeName);

        // 1. Ticker
        std::cout << "[Ticker] Fetching..." << std::endl;
        try {
            Ticker ticker = exchange.fetchTicker(symbol);
            std::cout << "  Bid: " << ticker.bidPrice << " | Ask: " << ticker.askPrice
                      << " | Last: " << ticker.lastPrice << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 2. OrderBook
        std::cout << "[OrderBook] Fetching..." << std::endl;
        try {
            OrderBook book = exchange.fetchOrderBook(symbol, 5);
            std::cout << "  Bids: " << book.bids.size() << " | Asks: " << book.asks.size() << std::endl;
            if (!book.bids.empty()) std::cout << "  Top Bid: " << book.bids[0].price << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 3. Trades
        std::cout << "[Trades] Fetching..." << std::endl;
        try {
            auto trades = exchange.fetchTrades(symbol, 5);
            std::cout << "  Count: " << trades.size() << std::endl;
            if (!trades.empty()) {
                std::cout << "  Last Trade: " << trades[0].price << " (" << trades[0].side << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 4. OHLCV
        std::cout << "[OHLCV] Fetching..." << std::endl;
        try {
            auto candles = exchange.fetchOHLCV(symbol, "60", 5); // 1 min candles
            std::cout << "  Count: " << candles.size() << std::endl;
            if (!candles.empty()) {
                std::cout << "  Last Close: " << candles[0].close << " @ " << candles[0].timestamp << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR initializing " << exchangeName << ": " << e.what() << std::endl;
    }
}

namespace ccapi {
    Logger* Logger::logger = nullptr;
}

int main() {
    // Disable detailed logs for clean output unless needed
    ccapi::Logger* logger = new ccapi::Logger();
    ccapi::Logger::logger = logger;

    struct ExchangeTarget {
        std::string name;
        std::string symbol;
    };

    std::vector<ExchangeTarget> targets = {
        {"binance-us", "BTCUSDT"},
        {"coinbase", "BTC-USD"},
        {"kraken", "XXBTZUSD"}, // Kraken quirk: standard symbols often map, but sometimes alt names used. Let's try standard pair name first if CCAPI handles it? CCAPI usually expects normalized or exchange specific. Kraken usually uses XBT/USD.
        // Let's try "XBT/USD" or "XXBTZUSD". CCAPI docs examples use "XBT/USD" or "ETH/USD"? No, example says "BTC-USDT" for OKX.
        // For Kraken, CCAPI `ccapi_market_data_service_kraken.h` comments?
        // Usually Kraken pairs are "XBT/USD" in Websockets, but REST might need "XXBTZUSD".
        // Let's try "XBT/USD".
        {"gateio", "BTC_USDT"},
        {"kucoin", "BTC-USDT"},
        {"gemini", "btcusd"},
        {"bitstamp", "btcusd"},
        {"huobi", "btcusdt"}, // Huobi might be blocked
        {"okx", "BTC-USDT"}, // OKX might be blocked
    };

    for (const auto& target : targets) {
        testExchange(target.name, target.symbol);
    }

    return 0;
}
