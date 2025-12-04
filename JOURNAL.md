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
    - Séparation du code en deux parties :
        1. `src/sessions/` : Instanciation lourde de `ccapi::Session`. Ces fichiers ne changent presque jamais.
        2. `src/exchanges/` : Logique métier (parsing, requêtes). Ces fichiers sont légers.
    - **Résultat** : Le temps de recompilation d'un fichier de logique (ex: `binance.cpp`) est passé de **~1m30s** à **~3s**.
    - Automatisation de la migration pour les 30 exchanges via un script Python.

## Prochaines Étapes
- Investiguer et corriger les implémentations de `Bybit` et `Binance.US`.
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades).
