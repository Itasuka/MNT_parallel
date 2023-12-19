Travail réalisé par Badet Maxime et Schivre Nicolas

Comme supposé lors du rendu de 12h, il sagissait bien de conditions oubliées
dans le calcul des bassins versant, donc dans la fonction f_bassin.
En effet, si on a un puit dans les ghosts, il ne faut pas le calculer sa
valeur de bassin car c'est au processus qui nous a envoyé le ghost de le
faire. On se contente de récupérer la valeur du bassin déjà existente (on
attend que le processus concerné calcule sa valeur et nous l'envoie).
