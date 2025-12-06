#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include "nccapi/client.hpp"

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"

void print_candle_vector(const std::vector<nccapi::Candle>& candles, int limit = 5) {
    if (candles.empty()) {
        std::cout << YELLOW << "No candles returned." << RESET << std::endl;
        return;
    }

    std::cout << "Received " << candles.size() << " candles." << std::endl;
    int count = 0;
    for (const auto& candle : candles) {
        if (count >= limit && count < candles.size() - limit) {
            if (count == limit) std::cout << "..." << std::endl;
            count++;
            continue;
        }
        std::cout << candle.toString() << std::endl;
        count++;
    }
}

int main(int argc, char* argv[]) {
    nccapi::Client client;
    std::srand(std::time(nullptr));

    std::vector<std::string> exchanges_to_test;
    if (argc > 1) {
        exchanges_to_test.push_back(argv[1]);
    } else {
        // Default to testing a safe exchange first, or all supported
        exchanges_to_test.push_back("coinbase");
    }

    for (const auto& exchange_name : exchanges_to_test) {
        std::cout << CYAN << "==========================================" << RESET << std::endl;
        std::cout << CYAN << "Testing exchange: " << exchange_name << RESET << std::endl;
        std::cout << CYAN << "==========================================" << RESET << std::endl;

        try {
            // 1. Get Pairs
            std::cout << "Fetching instruments..." << std::endl;
            auto instruments = client.get_pairs(exchange_name);
            std::cout << GREEN << "Success. Found " << instruments.size() << " instruments." << RESET << std::endl;

            if (instruments.empty()) {
                std::cerr << RED << "Skipping historical test (no instruments found)." << RESET << std::endl;
                continue;
            }

            // 2. Pick a random pair (preferably a major one like BTC/USDT or BTC-USD to ensure liquidity/history)
            // We search for "BTC" and "USD" to increase odds of valid data.
            nccapi::Instrument selected_inst;
            bool found = false;
            // First pass: Try exact matches for common major pairs
            std::vector<std::string> priorities = {"BTC/USDT", "BTC-USDT", "BTC/USD", "BTC-USD", "ETH/USDT", "ETH-USDT", "ETH/USD", "ETH-USD"};

            for (const auto& target : priorities) {
                for (const auto& inst : instruments) {
                    if (inst.symbol == target || inst.id == target) {
                        selected_inst = inst;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }

            if (!found) {
                for (const auto& inst : instruments) {
                    if ((inst.symbol.find("BTC") != std::string::npos || inst.symbol.find("ETH") != std::string::npos) &&
                        (inst.symbol.find("USD") != std::string::npos)) {
                        selected_inst = inst;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) selected_inst = instruments[rand() % instruments.size()];

            std::cout << "Selected Instrument: " << selected_inst.symbol << " (ID: " << selected_inst.id << ")" << std::endl;

            // 3. Fetch Historical Data
            std::cout << "Fetching historical candles (1m)..." << std::endl;

            // Lookback 1 hour
            auto now = std::chrono::system_clock::now();
            auto one_hour_ago = now - std::chrono::hours(1);
            int64_t to_ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            int64_t from_ts = std::chrono::duration_cast<std::chrono::milliseconds>(one_hour_ago.time_since_epoch()).count();

            auto candles = client.get_historical_candles(exchange_name, selected_inst.id, "1m", from_ts, to_ts);

            // 4. Validate
            if (!candles.empty()) {
                 std::cout << GREEN << "Success! " << RESET << std::endl;
                 print_candle_vector(candles);

                 // Basic consistency check
                 bool consistent = true;
                 for (size_t i = 1; i < candles.size(); ++i) {
                     if (candles[i].timestamp < candles[i-1].timestamp) {
                         std::cerr << RED << "Error: Candles are not sorted by timestamp!" << RESET << std::endl;
                         consistent = false;
                         break;
                     }
                 }
                 if (consistent) std::cout << GREEN << "Data consistency check passed." << RESET << std::endl;

            } else {
                std::cout << YELLOW << "Warning: Returned empty candle vector (might be inactive pair or API limitation)." << RESET << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << RED << "Test Failed: " << e.what() << RESET << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
