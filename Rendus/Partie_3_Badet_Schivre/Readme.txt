Travail réalisé par Badet Maxime et Schivre Nicolas

Tout d'abord on créer et initialise un tableau de bassin local sur chaque
processus (de la même taille que les autres tableaux locaux).
Ensuite on initialise à l'intérieur les valeurs à -1 et no_bassin_value sur
les ghosts supérieur du processus 0 et inférieur du processus mpi_size-1.

Ensuite, on doit partager le nombre de puits que chaque processus possède sur
son tableau local. On partage ensuite ces valeurs dans un tableau (à l'aide de
MPI_Allgather).
Ce tableau nous permet de savoir à partir de quel numéro chaque processus va
commencer à numéroter les bassins (la somme du nombre de puits de tous les
processus précédent).

On fait ensuite une boucle while similaire au calcul des flots, c'est-à-dire
avec la même condition de sortie. En effet, on sort quand plus aucun processus ne fait
de calcul, donc quand aucune valeur du tableau n'est égale à -1 (la valeur par
défaut).

Dans chaque tour de boucle on effectue la fonction calcul_bassin avec comme
argument le bassin local et les directions locales en déplaçant les pointeurs
à la 2e ligne (+nb_cols) pour ne pas calculer les ghosts.
Dans la fonction f_bassin on a rajouter une condition pour ne pas chercher la
valeur de bassin versant d'une valeur en dehors du tableau car avec les ghosts
on peut se retrouver, à cause de l'appel récursif, à chercher des valeurs dans
des zones mémoire non allouées. Comme on fait des séparations en bandes, le
problèmes est lié au i (ligne).

On a encore quelques problèmes de calcul concernant les bassins versant au
travers des ghosts. En effet, au travers des ghosts, la valeur de bassin n'est pas
toujours la bonne. On suppose qu'il y a un problème concernant notre calcul de
direction (une condition oublié,...).
