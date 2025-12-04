#include "nccapi/exchanges/binance.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

int main() {
    nccapi::Binance exchange;
    std::cout << "Fetching instruments for Binance..." << std::endl;
    auto instruments = exchange.get_instruments();
    std::cout << "Found " << instruments.size() << " instruments." << std::endl;

    std::ofstream out("analysis/binance.txt");
    if (instruments.empty()) {
        std::cerr << "No instruments found!" << std::endl;
        return 1;
    }

    // Dump first 5 and last 5 for brevity in log, but all to file?
    // User wants to analyze structures. Let's dump all keys found across all instruments,
    // and a few sample full dumps.

    // Collect all unique keys
    std::map<std::string, int> keys_frequency;
    for (const auto& instr : instruments) {
        for (const auto& kv : instr.info) {
            keys_frequency[kv.first]++;
        }
    }

    out << "Keys Stats:" << std::endl;
    for (const auto& kv : keys_frequency) {
        out << kv.first << ": " << kv.second << " occurrences" << std::endl;
    }
    out << "------------------------------------------------" << std::endl;

    // Dump first 10 instruments fully
    for (size_t i = 0; i < std::min((size_t)10, instruments.size()); ++i) {
        out << "Instrument " << i << ":" << std::endl;
        out << "  ID: " << instruments[i].id << std::endl;
        out << "  Symbol: " << instruments[i].symbol << std::endl;
        out << "  Base: " << instruments[i].base << std::endl;
        out << "  Quote: " << instruments[i].quote << std::endl;
        out << "  TickSize: " << instruments[i].tick_size << std::endl;
        out << "  StepSize: " << instruments[i].step_size << std::endl;
        out << "  Info:" << std::endl;
        for (const auto& kv : instruments[i].info) {
            out << "    " << kv.first << ": " << kv.second << std::endl;
        }
        out << std::endl;
    }

    out.close();
    std::cout << "Analysis saved to analysis/binance.txt" << std::endl;
    return 0;
}
