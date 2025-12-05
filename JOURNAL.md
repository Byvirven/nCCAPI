# Journal de Bord - nCCAPI Wrapper

## 1. Initialisation & Exploration
- Mise en place de l'environnement de développement.
- Analyse des dépendances : CCAPI (sous-module), Boost, RapidJSON, OpenSSL.
- Contraintes identifiées : Compilation très lourde en mémoire (interdiction du parallélisme `make -j`).

## 2. Refactorisation Majeure : Architecture Client/Exchange
- **Objectif** : Abandonner l'ancienne classe monolithique `UnifiedExchange` au profit d'une architecture modulaire.
- **Réalisation** :
    - Création de `nccapi::Client` comme point d'entrée unique.
    - Création de l'interface `nccapi::Exchange`.
    - Implémentation du pattern Pimpl.

## 3. Implémentation `GET_INSTRUMENTS`
- Implémentation réussie de `get_instruments()` pour la quasi-totalité des exchanges.

## 4. Résolution du Problème Binance US
- **Problème** : Erreur `-1104` lors de la récupération des instruments.
- **Solution** : Remplacement de l'appel standard par une `GENERIC_PUBLIC_REQUEST` manuelle vers `/api/v3/exchangeInfo`, avec parsing JSON robuste.
- **Résultat** : Fonctionnel (612 instruments).

## 5. Optimisation Finale de l'Architecture (Unified Session)
- **Constat** : L'approche "Une session par exchange" (optimisation initiale) restait trop lente à compiler globalement (30 fichiers x 1m30s = 45 minutes).
- **Innovation** : Transition vers une architecture **Unified Session**.
    - Une seule classe `UnifiedSession` instancie `ccapi::Session` avec **tous** les exchanges activés.
    - Ce fichier unique (`src/sessions/unified_session.cpp`) prend environ **2m30s** à compiler.
    - Tous les autres fichiers (`src/exchanges/*.cpp`) sont légers et compilent en **~3 secondes** chacun.
    - `nccapi::Client` instancie cette session unique et la partage (Dependency Injection) avec toutes les instances d'`Exchange`.
- **Gain de Performance** : Temps de compilation total pour un rebuild complet passé de **~45 minutes** à **~4 minutes**.

## 6. Correctifs Critiques (Instruments Fetching)
- **Bitmex** : Crash Core Dump résolu par parsing manuel (`GENERIC_PUBLIC_REQUEST`).
- **Kraken Futures** : Crash Core Dump résolu par parsing manuel.
- **Bitget Futures** : Passage de 0 à 648 instruments via itération des `productType`.
- **GateIO Perpetual Futures** : Passage de 0 à 592 instruments via itération des devises de règlement (`settle`).
- **ErisX** : Désactivé suite à la migration vers Cboe Digital.
- **Bybit** : Mise à jour vers l'API V5 (itération des catégories), mais validation impossible (Geoblocking).
- **Binance USDS Futures** : Diagnostiqué comme fonctionnel mais Geo-bloqué (Erreur 451).

## 7. Structure de Données (Instrument)
- Enrichissement de la structure `Instrument` pour supporter les produits dérivés :
    - `settle` (Devise de règlement)
    - `expiry` (Date d'expiration)
    - `strike_price` (Prix d'exercice)
    - `option_type` (Call/Put)
    - `contract_size` / `contract_multiplier`
- Ajout de `toString()` pour le débogage.
- Mise à jour des parsers manuels (Bitmex, Kraken, etc.) pour peupler ces nouveaux champs.

## Prochaines Étapes
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades) en utilisant cette nouvelle architecture unifiée.
- Ajouter le support WebSocket complet.
