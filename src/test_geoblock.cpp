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
    "binance", "gateio", "bybit", "whitebit", "deribit", "bitmex", "cryptocom", "kucoin"
};

bool isValidPair(const Instrument& i) {
    std::string quote = i.quoteAsset;
    if (quote == "USDT" || quote == "USD" || quote == "BTC" || quote == "ETH") return true;
    return false;
}

void runTestForExchange(const std::string& exchangeName, std::ofstream& report) {
    std::cout << "\n========================================\n";
    std::cout << "TESTING (Geoblock): " << exchangeName << "\n";
    std::cout << "========================================\n";

    report << "Exchange: " << exchangeName << "\n";

    try {
        UnifiedExchange exchange(exchangeName);

        std::cout << "[Step 1] Fetching Instruments...\n";
        auto instruments = exchange.fetchInstruments();
        std::cout << "  Found " << instruments.size() << " instruments.\n";

        if (instruments.empty()) {
            report << "  - CRITICAL: No instruments found. Aborting.\n";
            return;
        }

        std::vector<Instrument> candidates;
        for (const auto& i : instruments) {
            if (isValidPair(i)) candidates.push_back(i);
        }
        if (candidates.empty()) candidates = instruments;

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

            try {
                auto ticker = exchange.fetchTicker(symbol);
                if (ticker.lastPrice > 0) res.tickerRest = true;
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
                auto ohlcv = exchange.fetchOHLCV(symbol, "60", 100);
                if (!ohlcv.empty()) res.ohlcvRest = true;
                std::cout << "    [REST] OHLCV: " << ohlcv.size() << " candles\n";
            } catch (const std::exception& e) { std::cerr << "    [REST] OHLCV Failed: " << e.what() << "\n"; res.notes += "OHLCVREST Fail; "; }

            // WS
            std::atomic<int> tickerCount{0};
            exchange.setOnTicker([&](const Ticker& t) { tickerCount++; });
            exchange.subscribeTicker(symbol);

            std::cout << "    [WS] Listening for 30s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (tickerCount > 0) res.tickerWs = true;

            report << "    REST: Ticker=" << res.tickerRest << " Book=" << res.bookRest << "\n";
            report << "    WS  : Ticker=" << res.tickerWs << "\n";
            report << "--------------------------------------------------\n";
        }

    } catch (const std::exception& e) {
        report << "  CRITICAL ERROR: " << e.what() << "\n";
    }
}

int main() {
    std::ofstream report("report_geoblock.txt");
    for (const auto& ex : EXCHANGES) {
        runTestForExchange(ex, report);
    }
    report.close();
    return 0;
}
