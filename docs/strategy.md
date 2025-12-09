# Stratégie de l'IA Gomoku (version actuelle)

## Priorités

- **Gagner immédiatement** : si un coup fait 5 alignés, on le joue.
- **Bloquer immédiatement** : si l’adversaire peut faire 5, on bloque.
- **Fenêtre restreinte** : on ne cherche que dans une bande de 2 autour des pierres déjà posées pour éviter les coups hors-jeu.
- **Menace forcée courte** : on cherche un coup qui garantit une victoire au tour suivant, même après la meilleure réponse adverse (threat search 2-plis local).

## Heuristique d’évaluation

- **Patterns ouverts/fermés** : scoring fort pour 4 ouverts, 4 fermés, 3 ouverts/fermés, 2 ouverts/fermés (attaque et défense).
- **Menaces éclatées** : détection de `XX.XX` ou `XX.X` avec au moins une extrémité ouverte (menace cachée, non évidente).
- **Fork / double menace** : bonus si le coup est fort dans deux directions.
- **Proximité** : bonus si le coup est proche de pierres existantes (rayon 2).
- **Centralité** : bonus si le coup est proche du centre.

## Pseudocode de décision

```pseudocode
if board_is_empty:
	return center

bounds = compute_bounds(board, margin=2)

# 1) Threat search (victoire forcée en 2-plis)
forced = threat_search_forced_win(bounds)
if forced exists:
	return forced

best_score = -inf; best_move = none
for each empty cell in bounds:
	if makes_win(me): return cell
	if blocks_win(opponent): take it with high score
	score = evaluate_position(cell)
	keep best

if best_move is none:
	play closest_to_center
return best_move
```

`evaluate_position(cell)` combine :

- `pattern_score` (2/3/4/5 ouverts/fermés) en attaque.
- `gapped_threat_score` (split threats) en attaque.
- Même motifs en défense (poids réduits) pour bloquer.
- Bonus fork (meilleurs 2 axes), proximité, centralité.

`threat_search_forced_win(bounds)` :

1. Pour chaque coup candidat :
   - Si coup gagnant direct → retourner.
   - Sinon, simuler notre coup.
   - Pour chaque réponse locale de l’adversaire :
	   - Si l’adversaire gagne direct → ce coup n’est pas forcé.
	   - Sinon, vérifier qu’on a une réplique gagnante immédiate.
   - Si aucune réponse adverse ne nous empêche de gagner au coup suivant → jouer ce coup.

## Idées futures

- Alpha-bêta top-K pour voir 2–4 coups plus loin.
- Table de transposition (hash Zobrist).
- Détection de doubles menaces forcées plus profonde.
- Pénalisation des coups qui ouvrent une 4 ouverte pour l’adversaire.
