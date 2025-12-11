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

## 4. Optimisation Finale de l'Architecture (Unified Session)
- **Constat** : L'approche "Une session par exchange" (optimisation initiale) restait trop lente à compiler globalement (30 fichiers x 1m30s = 45 minutes).
- **Innovation** : Transition vers une architecture **Unified Session**.
    - Une seule classe `UnifiedSession` instancie `ccapi::Session` avec **tous** les exchanges activés.
    - Ce fichier unique (`src/sessions/unified_session.cpp`) prend environ **2m30s** à compiler.
    - Tous les autres fichiers (`src/exchanges/*.cpp`) sont légers et compilent en **~3 secondes** chacun.
    - `nccapi::Client` instancie cette session unique et la partage (Dependency Injection) avec toutes les instances d'`Exchange`.
- **Gain de Performance** : Temps de compilation total pour un rebuild complet passé de **~45 minutes** à **~4 minutes**.

## 5. Correctifs et Stabilisation (Validation Finale)
- **Correctifs Critiques** :
    - **Bitmex / Kraken Futures** : Passage au parsing manuel (`GENERIC_PUBLIC_REQUEST`) pour éviter les crashs liés aux assertions JSON.
    - **Bitget Futures / GateIO Perpetual** : Correction de la récupération vide (0 paires) par itération des paramètres requis (`productType`, `settle`).
    - **Binance US** : Correction de l'erreur `-1104` et du crash de destruction (fixé via `session->stop()`).
    - **Bitmart** : Correction de la récupération des bougies historiques (0 résultat) causée par un type de timestamp string au lieu de number dans l'API V3.
    - **CMake** : Mise à jour du `CMakeLists.txt` vers la version 3.24 pour supporter correctement `DOWNLOAD_EXTRACT_TIMESTAMP` lors du téléchargement des dépendances (Boost/RapidJSON).
- **Structure de Données** : Enrichissement de `Instrument` pour supporter les futures/options (`expiry`, `strike`, `settle`, `type`).
- **Validation** :
    - Tests locaux de l'utilisateur confirmant le fonctionnement à 100% de tous les exchanges (y compris `binance`, `bybit`, etc. qui sont geobloqués en cloud).
    - Tests cloud validant les fixes de crash et l'architecture.
    - Vérification complète de la compilation "from scratch" avec téléchargement automatique des dépendances.
    - Analyse des APIs pour identifier les exchanges retournant des données historiques incomplètes ("gaps"). Bitfinex, Bitstamp, Coinbase, Gemini, et Kraken ont été identifiés.

## Prochaines Étapes
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades).
- Ajouter le support WebSocket complet en utilisant l'Unified Session.
