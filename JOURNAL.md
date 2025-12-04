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

## 4. Optimisation Drastique du Temps de Compilation
- **Problème** : Chaque modification de la logique d'un exchange entraînait la recompilation de tout le fichier, y compris les templates CCAPI (1m30s par fichier).
- **Solution** : Adoption de la stratégie "Reduce Build Time" recommandée par CCAPI.
    - Séparation du code en deux parties : `src/sessions/` (lourd) et `src/exchanges/` (léger).
    - Automatisation de la migration via un script Python.

## 5. Résolution du Problème Binance US
- **Problème** : L'implémentation standard de CCAPI pour `GET_INSTRUMENTS` sur Binance US échouait avec une erreur `-1104` ("Not all sent parameters were read"). Cela était dû à l'ajout automatique du paramètre `showPermissionSets=false` par CCAPI, que l'API Binance US (contrairement à Binance Global) ne semble pas supporter ou traiter correctement dans ce contexte.
- **Diagnostic** :
    - Utilisation d'un test isolé (`test_binance_us_iso`) pour reproduire l'erreur.
    - Confirmation que l'erreur provenait de l'appel REST standard.
    - Identification d'une dépendance manquante : `CCAPI_ENABLE_EXCHANGE_BINANCE` était requise en plus de `CCAPI_ENABLE_EXCHANGE_BINANCE_US` pour compiler correctement la session (car `BinanceUs` hérite de `BinanceBase`).
- **Solution** :
    1.  **Architecture Session** : Ajout de `CCAPI_ENABLE_EXCHANGE_BINANCE` dans les définitions de compilation de `src/sessions/binance-us_session.cpp` via `CMakeLists.txt`.
    2.  **Logique Request** : Remplacement de l'opération standard `GET_INSTRUMENTS` par une requête manuelle `GENERIC_PUBLIC_REQUEST` ciblant `/api/v3/exchangeInfo`.
    3.  **Parsing** : Parsing manuel de la réponse JSON avec RapidJSON pour extraire la liste des symboles.
- **Résultat** : La méthode `get_instruments()` pour Binance US fonctionne désormais correctement et retourne la liste des paires (612 instruments trouvés lors du test).

## Prochaines Étapes
- Investiguer et corriger l'implémentation de `Bybit`.
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades).
