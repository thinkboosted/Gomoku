# Stratégie de l'IA Gomoku (version actuelle)

## Priorités

- **Gagner immédiatement** : si un coup fait 5 alignés, on le joue.
- **Bloquer immédiatement** : si l’adversaire peut faire 5, on bloque.
- **Bloquer les 4 ouverts / menaces éclatées fortes** : si un coup empêche une 4 ouverte adverse (ou `XX.XX`), on le prend avant la heuristique.
- **Fenêtre restreinte** : on ne cherche que dans une bande de 2 autour des pierres déjà posées (3 si le plateau > 12) pour éviter les coups hors-jeu.
- **Menace forcée courte** : on cherche un coup qui garantit une victoire au tour suivant, même après la meilleure réponse adverse (threat search 2-plis local).
- **Menace forcée adverse** : si l’adversaire a un 2-plis forcé, on joue sur son coup de départ pour casser la séquence.
- **Garde-fou tactique** : on évite (ou pénalise fortement) un coup qui laisse un gain immédiat à l’adversaire au tour suivant.

## Heuristique d’évaluation

- **Patterns ouverts/fermés** : scoring fort pour 4 ouverts, 4 fermés, 3 ouverts/fermés, 2 ouverts/fermés (attaque et défense).
- **Menaces éclatées** : détection de `XX.XX` ou `XX.X` avec au moins une extrémité ouverte (menace cachée, non évidente).
- **Extension espacée** : bonus léger pour jouer à 3 cases d'une pierre amie (`X00X`) si les cases intermédiaires sont libres et au moins une extrémité reste ouverte.
- **Fork / double menace** : bonus si le coup est fort dans deux directions.
- **Proximité** : bonus si le coup est proche de pierres existantes (rayon 2), mais plafonné pour éviter les coups “dans le tas”.
- **Centralité** : bonus près du centre surtout en début de partie (poids réduit quand le plateau se remplit).

## Pseudocode de décision

```pseudocode
if board_is_empty:
	return center

bounds = compute_bounds(board, margin=2)

# 1) Threat search (victoire forcée en 2-plis)
forced = threat_search_forced_win(bounds)
if forced exists:
	return forced

# 1b) Threat search défensif
opp_forced = threat_search_forced_win_as_opponent(bounds)
if opp_forced exists:
	return opp_forced

best_score = -inf; best_move = none
for each empty cell in bounds:
	if makes_win(me): return cell
	if blocks_win(opponent): take it with high score
	if blocks open_four_or_hidden_four(opponent): take it before generic heuristic
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
