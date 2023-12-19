Rendu des flots d'accumulations par Badet Maxime et Schivre Nicolas

Initialisation d'un tableau de flots locaux sur chaque processus.

Initialisation de 4 int utilisés comme des booléens (servant à partager des
informations avec les autres processus):
- calculating : 1 si le processus continue le calcul de flots, 0 sinon
- anc_calculating : dernière version possédée par les autres processus de
  calculating
- north_calculating : valeur de calculating du processus au dessus
- south_calculating : valeur de calculating du processus en dessous

Tous ces entiers sont initialisés à 1.
On fait ensuite une boucle pour calculer le flot tant que le processus doit
calculer son flot ou qu'un de ses voisins le calcul:
- Si le processus doit faire le calcul alors on met calculating à 0, on fait
  le calcul de flot, et si on a sauté un calcul car il nous manque des
informations alors on remet calculating à 1 afin de refaire une itération.
- Si on a fait le calcul des flots, on re-envoie les ghosts aux processus
  voisins qui font eux aussi les calculs
- Ensuite, à l'aide de anc_calculating et de north ou south calculating, on
  peut savoir si un processus voisin nous envoie son flot, dans ce cas on fait
un MPI_Recv
- Enfin on renvoie notre nouvelle valeur de calculating au voisin si elle était à 1 et
  on reçoit celle des voisins si elle était à 1. On met à jour anc_calculating
avec la valeur envoyée aux voisins.


Après le calcul, le processus 0 va gather tous les flots locaux dans un
tableau de flot global.
