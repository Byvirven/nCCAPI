# Journal de Bord - Wrapper CCAPI Normalisé

## Initialisation du Projet
- **Date**: Aujourd'hui
- **Objectif**: Créer un wrapper C++ normalisé pour la bibliothèque CCAPI (Crypto-Chassis), inspiré par la simplicité de CCXT.
- **Actions**:
    - Initialisation du dépôt.
    - Planification de l'architecture.
    - Clonage de la dépendance CCAPI.

## Configuration du Projet
- **Actions**:
    - Téléchargement des dépendances header-only : Boost (1.87.0) et RapidJSON (1.1.0) dans `external/include`.
    - Création du fichier `CMakeLists.txt` configurant les chemins d'inclusion et les flags de compilation.
    - Activation des services MARKET_DATA et EXECUTION_MANAGEMENT.
    - Test de compilation basique réussi avec `src/main.cpp`.
    - Correction du `CMakeLists.txt` pour inclure correctement les headers de Boost.
    - Correction du linking de `ccapi::Logger::logger` dans `src/main.cpp`.

## Implementation - Public API
- **Actions**:
    - Creation de `src/UnifiedExchange.hpp`.
    - Implémentation de `fetchTicker` et `fetchOrderBook` en utilisant `ccapi::Session`.
    - Test avec `binance` (Erreur 451 Restricted Location).
    - Bascule vers `binance-us` et `BTCUSDT`.
        - `fetchTicker` fonctionne (Bid=91202.7, Ask=91411.4).
        - `fetchOHLCV` fonctionne (5 candles).
        - `fetchOrderBook` fonctionne (5 bids) après implémentation du fallback Generic Request.
    - `coinbase`.
        - `fetchTicker` fonctionne (Bid=91259.7 Ask=91259.8) grâce au Generic Request.
        - `fetchOrderBook` fonctionne aussi grâce au Generic Request implémenté (même logique que Ticker).

## Implementation - Private API
- **Actions**:
    - Ajout de `createOrder` et `fetchBalance` dans `UnifiedExchange.hpp`.
    - Utilisation des credentials passés dans le constructeur.
    - Normalisation des paramètres (SIDE uppercase, credentials injection).
    - Le code est prêt mais non testable (pas de clés).

## Conclusion
Le wrapper offre une interface unifiée robuste. Il détecte automatiquement les cas où l'implémentation standard CCAPI échoue (Coinbase Ticker/OrderBook, BinanceUS OrderBook crash) et utilise des requêtes génériques adaptées. La gestion des événements synchrones a été fiabilisée avec une boucle d'attente et un filtrage par `correlationId`.
