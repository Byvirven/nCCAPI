#include <iostream>
#include <vector>
#include "nccapi/client.hpp"

int main() {
    nccapi::Client client;

    std::vector<std::string> exchanges = client.get_supported_exchanges();
    std::cout << "Supported exchanges: ";
    for (const auto& name : exchanges) std::cout << name << " ";
    std::cout << std::endl;

    for (const auto& exchange : exchanges) {
        std::cout << "\nFetching pairs for " << exchange << "..." << std::endl;
        try {
            auto pairs = client.get_pairs(exchange);
            std::cout << "Received " << pairs.size() << " pairs." << std::endl;
            if (!pairs.empty()) {
                std::cout << "Sample: " << pairs[0].toString() << std::endl;
                std::cout << "        " << pairs[0].symbol << " Tick: " << pairs[0].tick_size << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error fetching pairs for " << exchange << ": " << e.what() << std::endl;
        }
    }

    return 0;
}
