Travail réalisé par Badet Maxime et Schivre Nicolas

La première étape après la lecture du fichier était d'initialiser les
différentes dimensions ainsi que les tableaux.
Comme le processus 0 est le seul à avoir fait la lecture de fichier, il est
par conséquent le seul à posséder la valeur du nombre de lignes et de
colonnes. Nous avons donc fait un Bcast afin de partager cette valeur aux
autres processus.
On a ensuite, à partir des tailles précédémment partagées, calculé la taille
des tableaux locaux. nous avons alloué nos tableaux avec une taille + 2 afin de
prendre en compte les 2 lignes de ghosts.

Pour remplir les tableaux de terrain locaux, nous avons fait un MPI_Scatter en
faisant bien attention à ne pas envoyer les no_values du tableau global et
mettre les valeurs reçues sur les ghosts.

Pour envoyer les ghosts, nous avons fait 2 envoies (un pour la ligne du haut
et un pour celle du bas). Les envoient sont non bloquant afin d'éviter les
interblocages. Les réceptions sont elle bloquantes pour assurer la réception
des terrains avant de commencer le calcul des directions.

Après l'envoi des ghosts il a fallu modifier le premier ghost du processus 0
et le dernier ghost du dernier processus car ces 2 processus possède une ligne
de no_values.
Pour faire ces lignes, le processus 0 va remplacer sa première ligne par des
no_value et envoie la valeur no_value au dernier processus. 
Le dernier processus fait pareil pour sa dernière ligne. 

Pour le calcul des directions, il faut bien faire attention aux paramètres
car, on va, de la même manière que pour les terrains locaux, laisser une ligne
au début et à la fin pour les ghosts.

On échange les ghosts de la même manière que pour les terrains.
On met ensuite en place des valeurs n'impactant pas le futur calcul des flots
pour la première ligne de ghost du processus 0 et la dernière ligne de ghost
du dernier processus comme ils ne sont pas censés avoir de ghosts à cet
endroit. Nous avons choisi comme valeur -1.

En vue d'avoir un affichage global des directions sur un seul processus, on
utilise MPI_Gather pour rassembler les directions locales sur le processus 0.

On affiche ensuite les directions globales pour le processus 0 et les
directions locales pour les autres processus (en créant une fonction
mnt_do_tile_mpi)

Commandes utilisées pour exécuter le variant:

-Pour la map 6x6 (petit.txt):
./run -k mnt -v mpi --with-tile mpi --mpirun "-np 3" -i 1 -s 6 -ts 6

-Pour la map 32x32 (jeu_essai.txt):
./run -k mnt -v mpi --with-tile mpi --mpirun "-np 4" -i 1 -s 32 -ts 32

-Pour la map 1025x1025 (grd_618360_6754408.txt):
./run -k mnt -v mpi --with-tile mpi --mpirun "-np 5" -i 1 -s 1025 -ts 1025

-Pour la map 1024x1024 (alpes.txt):
./run -k mnt -v mpi --with-tile mpi --mpirun "-np 8" -i 1 -s 1024 -ts 1024
