# Stratégie de l'IA Gomoku

## Priorités principales

- **Gagner tout de suite** : si un coup fait 5 alignés, il est joué en priorité.
- **Bloquer la perte immédiate** : si l’adversaire peut faire 5 au prochain tour, on bloque d’abord.
- **Fenêtre de recherche réduite** : on évalue surtout les cases autour des pierres existantes (bande de 2) pour éviter les coups inutiles loin du jeu.

## Heuristique d’évaluation (offense)

- **Patterns ouverts/fermés** : comptage des alignements dans les 4 directions, avec poids élevés pour 4 ouverts, 4 fermés, 3 ouverts/fermés, 2 ouverts/fermés.
- **Menaces éclatées** : détection de motifs `XX.XX` ou `XX.X` (split threats) qui préparent une victoire au tour suivant sans montrer un 4 aligné.
- **Fork/threat multiplicity** : bonus si le coup est fort dans au moins deux directions (création de double menace).
- **Proximité** : bonus si le coup se place près d’autres pierres (rayon 2) pour éviter les coups isolés.
- **Centralité** : bonus si la case est proche du centre, offrant plus de possibilités d’extension.

## Heuristique d’évaluation (défense)

- Les mêmes motifs sont scorés côté adversaire (poids légèrement réduits) pour prioriser les blocs des menaces fortes (4 ouverts/fermés, split threats adverses).

## Ordre de décision

1) Coup gagnant (5 alignés).
2) Blocage d’un coup perdant immédiat (adversaire ferait 5).
3) Coup au meilleur score heuristique (patterns + menaces éclatées + fork + proximité + centre).
4) Fallback : plus proche du centre si aucune autre option n’est retenue.

## Idées d’amélioration futures

- **Recherche alpha-bêta** avec top-K coups ordonnés par l’heuristique pour voir 2–4 coups plus loin.
- **Table de transposition** (hash Zobrist) pour réutiliser les évaluations de positions déjà vues.
- **Détection de doubles menaces forcées** (threat search) pour sécuriser les victoires en deux temps.
- **Évaluation de sûreté** : petite pénalisation des coups qui ouvrent une “4 ouverte” immédiate à l’adversaire.
