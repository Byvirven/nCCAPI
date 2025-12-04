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
- **Problème** : L'implémentation standard de CCAPI pour `GET_INSTRUMENTS` sur Binance US échouait avec une erreur `-1104` et crashait lors de la destruction de l'objet.
- **Diagnostic & Correctifs** :
    1.  **Erreur API (-1104)** : Utilisation d'une `GENERIC_PUBLIC_REQUEST` manuelle vers `/api/v3/exchangeInfo` pour éviter les paramètres par défaut de CCAPI incompatibles.
    2.  **Dépendance de Compilation** : Activation de `CCAPI_ENABLE_EXCHANGE_BINANCE` (Base) en plus de `_BINANCE_US` dans la session pour satisfaire les dépendances internes de CCAPI.
    3.  **Crash à la fermeture** : Ajout de `session->stop()` explicite dans le destructeur de `BinanceUsSession` avant le `delete` pour assurer un nettoyage propre des threads `boost::asio`.
- **Validation** : Test isolé réussi (612 instruments récupérés).
- **Performance** :
    - Compilation logique (`binance-us.cpp`) : **~14s** (vs ~3m pour le test complet initial).
    - Compilation session (`binance-us_session.cpp`) : **~3m** (plus lourd que la moyenne car double instanciation Base+US, mais ne se fait qu'une fois).
    - Comparatif : `binance.cpp` logique ~4s, session ~1m17s. L'overhead pour US est acceptable vu la complexité résolue.

## Prochaines Étapes
- Investiguer et corriger l'implémentation de `Bybit`.
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades).
