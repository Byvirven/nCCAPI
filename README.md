# Unified CCAPI Wrapper

Un wrapper C++ header-only normalisé pour la bibliothèque [Crypto-Chassis CCAPI](https://github.com/crypto-chassis/ccapi), conçu pour offrir une expérience développeur similaire à CCXT (interface unique, simple et unifiée) tout en conservant la performance du C++.

## Fonctionnalités

Ce wrapper normalise les appels API REST publics et privés pour une multitude d'exchanges. Il gère en interne les spécificités ("quirks") de chaque plateforme (ex: format des tickers, structure du carnet d'ordres) pour exposer des structures de données C++ standards.

### Méthodes Supportées

Toutes les méthodes sont synchrones et bloquantes (via polling interne) pour une simplicité d'utilisation maximale.

*   **Public REST**:
    *   `Ticker fetchTicker(symbol)` : Prix, Bid, Ask, Volume.
    *   `OrderBook fetchOrderBook(symbol, limit)` : Carnet d'ordres normalisé (bids/asks).
    *   `vector<Trade> fetchTrades(symbol, limit)` : Derniers trades.
    *   `vector<OHLCV> fetchOHLCV(symbol, timeframe)` : Bougies (Candlesticks).
    *   `vector<Instrument> fetchInstruments()` : Liste des paires disponibles.

*   **WebSocket**:
    *   `subscribeTicker(symbol)`
    *   `subscribeOrderBook(symbol, depth)`
    *   `subscribeTrades(symbol)`
    *   `subscribeOHLCV(symbol, interval)`
    *   Callbacks configurables via `setOnTicker`, `setOnOrderBook`, etc.

*   **Privé** (Nécessite API Key) :
    *   `string createOrder(symbol, side, amount, price)` : Création d'ordre (Limit ou Market).
    *   `map<string, double> fetchBalance()` : Solde des comptes.

## Exchanges Supportés & Testés

| Exchange | Ticker | OrderBook | Trades | Remarques |
|----------|--------|-----------|--------|-----------|
| **Binance US** | ✅ | ✅ | ✅ | Via Generic Fallback |
| **Coinbase** | ✅ | ✅ | ✅ | Via Generic Fallback |
| **Kraken** | ✅ | ✅ | ✅ | |
| **Kucoin** | ✅ | ✅ | ✅ | |
| **Huobi** | ✅ | ✅ | ✅ | |
| **Bitstamp** | ✅ | ✅ | ✅ | |
| **Gemini** | ✅ | ✅ | ✅ | |
| **OKX** | ✅ | ✅ | ✅ | |
| **Mexc** | ✅ | ✅ | ✅ | |
| **Bitfinex** | ✅ | ✅ | ✅ | |
| **AscendEx** | ✅ | ✅ | ✅ | |
| **Gate.io** | ✅ | ⚠️ | ✅ | Book vide parfois |
| **Bitget** | ✅ | ❌ | ✅ | |

*(Note: Binance Global et Bitmex sont supportés par le code mais souvent bloqués par IP dans les environnements cloud/sandbox)*.

## Dépendances

Le projet est autonome mais dépend de bibliothèques header-only téléchargées dans `external/include` :
*   **CCAPI** (Crypto-Chassis)
*   **Boost** (Asio, Beast...)
*   **RapidJSON**
*   **OpenSSL** (Doit être installé sur le système)

## Compilation

```bash
mkdir build && cd build
cmake ..
make
```

## Exemple d'Utilisation

```cpp
#include "src/UnifiedExchange.hpp"
#include <iostream>

using namespace unified_crypto;

int main() {
    // 1. Initialisation (Public)
    UnifiedExchange exchange("coinbase");

    // 2. Récupération du Ticker
    try {
        Ticker t = exchange.fetchTicker("BTC-USD");
        std::cout << "BTC/USD: " << t.lastPrice << " (Bid: " << t.bidPrice << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << std::endl;
    }

    // 3. Initialisation (Privé)
    ExchangeConfig config;
    config.apiKey = "VOTRE_KEY";
    config.apiSecret = "VOTRE_SECRET";
    // config.passphrase = "VOTRE_PASSPHRASE"; // Si requis (ex: Kucoin, Coinbase Pro)

    UnifiedExchange privateExchange("binance-us", config);

    // 4. Création d'un ordre
    // privateExchange.createOrder("BTCUSDT", "BUY", 0.001); // Market Order

    return 0;
}
```

## Structure du Code

*   `src/UnifiedExchange.hpp` : **Le Coeur du projet**. Contient toute la logique, les classes, et la gestion des cas particuliers (Generic Requests).
*   `src/main.cpp` : Harnais de test basique REST.
*   `src/test_global.cpp` : Scénario de test complet (REST + WS + Rapport) pour validation finale.

## Architecture "Generic Request"

Certains exchanges (comme Coinbase ou Binance US sur certains endpoints) ne répondent pas correctement aux macros standards de CCAPI (`GET_BBOS`, etc.) dans certaines versions ou configurations.
Pour garantir la fiabilité, ce wrapper implémente un système de fallback : si l'exchange est connu pour être "difficile", le wrapper construit manuellement une requête HTTP (`GENERIC_PUBLIC_REQUEST`) et parse le JSON brut avec RapidJSON, contournant ainsi les limitations de l'abstraction par défaut.
