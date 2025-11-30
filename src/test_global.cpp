#include "UnifiedExchange.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <mutex>

using namespace unified_crypto;

struct TestResult {
    std::string exchange;
    std::string symbol;
    bool tickerRest = false;
    bool bookRest = false;
    bool tradesRest = false;
    bool ohlcvRest = false;
    bool tickerWs = false;
    bool bookWs = false;
    bool tradesWs = false;
    bool ohlcvWs = false;
    std::string notes;
};

std::vector<std::string> EXCHANGES = {
    "binance-us", "coinbase", "kraken", "gateio", "kucoin",
    "gemini", "bitstamp", "huobi", "okx", "bitfinex",
    "ascendex", "bybit", "mexc", "whitebit", "bitget",
    "cryptocom", "deribit", "bitmex"
};

// Filter valid trading pairs (e.g., BTC/USDT, ETH/USD)
bool isValidPair(const Instrument& i) {
    std::string base = i.baseAsset;
    std::string quote = i.quoteAsset;
    // Basic filter
    if (quote == "USDT" || quote == "USD" || quote == "BTC" || quote == "ETH" || quote == "EUR") return true;
    return false;
}

void runTestForExchange(const std::string& exchangeName, std::ofstream& report) {
    std::cout << "\n========================================\n";
    std::cout << "TESTING: " << exchangeName << "\n";
    std::cout << "========================================\n";

    report << "Exchange: " << exchangeName << "\n";

    try {
        UnifiedExchange exchange(exchangeName);

        // 1. Fetch Instruments
        std::cout << "[Step 1] Fetching Instruments...\n";
        auto instruments = exchange.fetchInstruments();
        std::cout << "  Found " << instruments.size() << " instruments.\n";

        if (instruments.empty()) {
            report << "  - CRITICAL: No instruments found. Aborting.\n";
            return;
        }

        // 2. Select Random Pairs
        std::vector<Instrument> candidates;
        for (const auto& i : instruments) {
            if (isValidPair(i)) candidates.push_back(i);
        }
        if (candidates.empty()) candidates = instruments; // Fallback

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(candidates.begin(), candidates.end(), g);

        int pairsToTest = std::min((size_t)2, candidates.size());

        for (int k = 0; k < pairsToTest; ++k) {
            std::string symbol = candidates[k].symbol;
            std::cout << "\n  -- Testing Pair: " << symbol << " --\n";
            report << "  Symbol: " << symbol << "\n";

            TestResult res;
            res.exchange = exchangeName;
            res.symbol = symbol;

            // 3. REST History
            try {
                auto ticker = exchange.fetchTicker(symbol);
                if (ticker.lastPrice > 0 || ticker.bidPrice > 0) res.tickerRest = true;
                std::cout << "    [REST] Ticker: " << ticker.lastPrice << "\n";
            } catch (const std::exception& e) { std::cerr << "    [REST] Ticker Failed: " << e.what() << "\n"; res.notes += "TickerREST Fail; "; }

            try {
                auto book = exchange.fetchOrderBook(symbol, 5);
                if (!book.bids.empty()) res.bookRest = true;
                std::cout << "    [REST] Book: " << book.bids.size() << " bids\n";
            } catch (const std::exception& e) { std::cerr << "    [REST] Book Failed: " << e.what() << "\n"; res.notes += "BookREST Fail; "; }

            try {
                auto trades = exchange.fetchTrades(symbol, 5);
                if (!trades.empty()) res.tradesRest = true;
                std::cout << "    [REST] Trades: " << trades.size() << "\n";
            } catch (const std::exception& e) { std::cerr << "    [REST] Trades Failed: " << e.what() << "\n"; res.notes += "TradesREST Fail; "; }

            try {
                auto ohlcv = exchange.fetchOHLCV(symbol, "60", 1000); // 1000 candles
                if (!ohlcv.empty()) res.ohlcvRest = true;
                std::cout << "    [REST] OHLCV: " << ohlcv.size() << " candles\n";
            } catch (const std::exception& e) { std::cerr << "    [REST] OHLCV Failed: " << e.what() << "\n"; res.notes += "OHLCVREST Fail; "; }

            // 4. WebSocket Stream
            std::atomic<int> tickerCount{0};
            std::atomic<int> bookCount{0};
            std::atomic<int> tradeCount{0};
            std::atomic<int> ohlcvCount{0};

            exchange.setOnTicker([&](const Ticker& t) { tickerCount++; });
            exchange.setOnOrderBook([&](const OrderBook& b) { bookCount++; });
            exchange.setOnTrade([&](const Trade& t) { tradeCount++; });
            exchange.setOnOHLCV([&](const OHLCV& c) { ohlcvCount++; });

            std::cout << "    [WS] Subscribing...\n";
            try {
                exchange.subscribeTicker(symbol);
                exchange.subscribeOrderBook(symbol);
                exchange.subscribeTrades(symbol);
                exchange.subscribeOHLCV(symbol);
            } catch(...) { std::cerr << "    [WS] Subscribe Error\n"; }

            std::cout << "    [WS] Listening for 3 minutes...\n";
            std::this_thread::sleep_for(std::chrono::seconds(180));

            if (tickerCount > 0) res.tickerWs = true;
            if (bookCount > 0) res.bookWs = true;
            if (tradeCount > 0) res.tradesWs = true;
            if (ohlcvCount > 0) res.ohlcvWs = true;

            std::cout << "    [WS] Stats - Ticker: " << tickerCount << ", Book: " << bookCount
                      << ", Trade: " << tradeCount << ", OHLCV: " << ohlcvCount << "\n";

            // Report Line
            report << "    REST: Ticker=" << (res.tickerRest?"OK":"FAIL")
                   << " Book=" << (res.bookRest?"OK":"FAIL")
                   << " Trades=" << (res.tradesRest?"OK":"FAIL")
                   << " OHLCV=" << (res.ohlcvRest?"OK":"FAIL") << "\n";
            report << "    WS  : Ticker=" << (res.tickerWs?"OK":"FAIL")
                   << " Book=" << (res.bookWs?"OK":"FAIL")
                   << " Trades=" << (res.tradesWs?"OK":"FAIL")
                   << " OHLCV=" << (res.ohlcvWs?"OK":"FAIL") << "\n";
            if (!res.notes.empty()) report << "    Notes: " << res.notes << "\n";
            report << "--------------------------------------------------\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL WRAPPER ERROR: " << e.what() << "\n";
        report << "  CRITICAL ERROR: " << e.what() << "\n";
    }
}

int main() {
    std::ofstream report("report.txt");
    report << "Global Unified CCAPI Wrapper Test Report\n";
    report << "========================================\n\n";

    for (const auto& ex : EXCHANGES) {
        runTestForExchange(ex, report);
    }

    report.close();
    std::cout << "\nGlobal Test Complete. Check report.txt.\n";
    return 0;
}
