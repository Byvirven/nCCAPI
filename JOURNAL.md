# Journal de Bord - nCCAPI Wrapper

## 1. Initialisation & Exploration
- Mise en place de l'environnement de développement.
- Analyse des dépendances : CCAPI (sous-module), Boost, RapidJSON, OpenSSL.
- Contraintes identifiées : Compilation très lourde en mémoire (interdiction du parallélisme `make -j`).

## 2. Refactorisation Majeure : Architecture Client/Exchange
- **Objectif** : Abandonner l'ancienne classe monolithique `UnifiedExchange` au profit d'une architecture modulaire et extensible.
- **Réalisation** :
    - Création de `nccapi::Client` comme point d'entrée unique (Façade/Factory).
    - Création de l'interface `nccapi::Exchange` définissant le contrat standard (ex: `get_instruments`).
    - Implémentation du pattern **Pimpl** (Pointer to Implementation) pour chaque exchange. Cela permet d'isoler le code lourd de CCAPI dans les fichiers `.cpp` (`src/exchanges/`) et de garder les headers (`include/nccapi/exchanges/`) légers.

## 3. Implémentation `GET_INSTRUMENTS` (Refactorisation Actuelle)
- **Objectif** : Uniformiser la récupération de la liste des paires de trading (Instruments) pour tous les exchanges supportés.
- **Résultats** :
    - Implémentation réussie de `get_instruments()` pour la quasi-totalité des exchanges supportés par CCAPI (voir liste dans README).
    - La méthode retourne un vecteur normalisé de structures `Instrument` (Symbole, Base, Quote).
    - **Test de validation** : Binance (Global) est fonctionnel (validé hors environnement géobloqué).
- **Problèmes Identifiés** :
    - **Géoblocage** : Binance Global et Bybit sont inaccessibles depuis l'environnement de développement actuel.
    - **Dysfonctionnements** :
        - `Bybit` : Rencontre des erreurs lors de l'appel API.
        - `Binance.US` : Rencontre des erreurs spécifiques à traiter ultérieurement.

## 4. Documentation
- Réécriture complète du `README.md` pour refléter la nouvelle architecture (`nccapi::Client`).
- Mise à jour des instructions de compilation (avertissement strict sur la mémoire).
- Classification claire des exchanges supportés vs problématiques.

## Prochaines Étapes
- Investiguer et corriger les implémentations de `Bybit` et `Binance.US`.
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades) dans la nouvelle architecture.
- Ajouter le support WebSocket via la nouvelle architecture.
