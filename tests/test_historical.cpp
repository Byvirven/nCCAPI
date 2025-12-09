#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <algorithm>
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
    std::string specific_pair = "";

    if (argc > 1) {
        exchanges_to_test.push_back(argv[1]);
    } else {
        exchanges_to_test = client.get_supported_exchanges();
    }

    if (argc > 2) {
        specific_pair = argv[2];
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

            // 2. Select Pair
            nccapi::Instrument selected_inst;
            bool found = false;

            if (!specific_pair.empty()) {
                for (const auto& inst : instruments) {
                    if (inst.symbol == specific_pair || inst.id == specific_pair) {
                        selected_inst = inst;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                     std::cout << YELLOW << "Specific pair '" << specific_pair << "' not found in instrument list. Trying fuzzy match..." << std::endl;
                     for (const auto& inst : instruments) {
                        if (inst.symbol.find(specific_pair) != std::string::npos || inst.id.find(specific_pair) != std::string::npos) {
                            selected_inst = inst;
                            found = true;
                            std::cout << "Fuzzy matched: " << inst.symbol << std::endl;
                            break;
                        }
                    }
                }
            }

            if (!found) {
                // First pass: Try exact matches for common major pairs
                std::vector<std::string> priorities = {
                    "BTC/USDT", "BTC-USDT", "BTC/USD", "BTC-USD", "XBT/USD", "XBTUSD",
                    "ETH/USDT", "ETH-USDT", "ETH/USD", "ETH-USD", "btcusdt", "btcusd"
                };

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
            }

            if (!found) {
                // Fallback to searching for BTC/USD substring
                for (const auto& inst : instruments) {
                    // Convert to upper for check
                    std::string sym = inst.symbol;
                    std::transform(sym.begin(), sym.end(), sym.begin(), ::toupper);
                    if ((sym.find("BTC") != std::string::npos) && (sym.find("USD") != std::string::npos)) {
                        selected_inst = inst;
                        found = true;
                        break;
                    }
                }
            }

            if (!found && !instruments.empty()) {
                 selected_inst = instruments[0];
                 found = true;
            }

            if (!found) {
                std::cout << RED << "Could not select a valid instrument." << RESET << std::endl;
                continue;
            }

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
