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
- **Gain de Performance** : Temps de compilation total pour un rebuild complet passé de **~45 minutes** à **~4-5 minutes**.

## 6. Correctifs Critiques (Instruments Fetching)
- **Bitmex** :
    - **Symptôme** : Crash violent (Core Dump) dû à une assertion RapidJSON `false`.
    - **Cause** : Le service standard CCAPI pour Bitmex semble échouer lors du parsing de certains champs manquants ou structures inattendues.
    - **Solution** : Remplacement par une requête `GENERIC_PUBLIC_REQUEST` vers `/api/v1/instrument/active` et parsing manuel sécurisé (vérification de l'existence des membres JSON avant accès).
    - **Résultat** : Récupération stable de 113 instruments.
- **Bitget Futures** :
    - **Symptôme** : 0 instruments retournés.
    - **Cause** : L'API V2 de Bitget nécessite obligatoirement le paramètre `productType` (ex: `USDT-FUTURES`). L'appel par défaut ne le fournissait pas.
    - **Solution** : Implémentation d'une boucle itérant sur les types de produits majeurs (`USDT-FUTURES`, `COIN-FUTURES`, `USDC-FUTURES`) pour récupérer l'ensemble des contrats.
    - **Résultat** : Récupération de 648 instruments.
- **Binance US (Status)** :
    - **Vérification** : Confirmation que le code parse correctement le champ `status`. La paire dépréciée `BTCUSD4` est bien marquée comme inactive (`active=false`) car son statut est `BREAK` (différent de `TRADING`).

## Prochaines Étapes
- Investiguer et corriger l'implémentation de `Bybit`.
- Implémenter les méthodes de Market Data (Ticker, OrderBook, Trades) en utilisant cette nouvelle architecture unifiée.
