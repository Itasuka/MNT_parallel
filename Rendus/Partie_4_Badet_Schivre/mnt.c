
#include "easypap.h"

#include <math.h>
#include <omp.h>


// Les données disponibles sont les suivantes
// Attention lors du lancement à bien signaler la taille pour que l'image générée soit correcte
// Par exemple pour la dalle grd il faudra ajouter -s 1025 à votre ligne de lancement
// Il y a également une valeur cst qui permet de générer une image relative aux flots d'accumulation.
//static char* filename = "data/mnt/petit.txt"; // ce fichier correspond à l'exemple de la fiche ipynb 6x6
//static int cst = 100;
static char* filename = "data/mnt/jeu_essai.txt"; // ce fichier correspond à un extrait 32 x 32 de la dalle grd
static int cst = 100;
//static char* filename = "data/mnt/grd_618360_6754408.txt"; // ce fichier correspond à la dalle grd complète 1025x1025
//static int cst = 10000;
//static char* filename = "data/mnt/alpes.txt"; // ce fichier correspond aux données sur les alpes 1024x1024
//static int cst = 10000;

static float no_value;
static int no_dir_value = -9;
static int no_bassin_value = -10;
static int nb_lignes;
static int nb_cols;

// Les tableaux pour constuire le terrain à partir du mnt et calculer les directions.
// Attention terrain est défini avec 2 lignes supplémentaires pour permettre des ghosts 
static float* terrain;
static int* direction;
static int* accumulation;
static int* bassin;

// Tableaux prédéfinis pour tester si un voisinage se déverse dans
// une cellule. Trois cas pour gérer le cas général et les 2 bords (droit et gauche)
static int dir_bord1[5]={5,6,7,8,1};
static int dir_bord2[5]={1,2,3,4,5};
static int dir_general[8]={5,6,7,8,1,2,3,4};

// Cette fonction permet de lire le fichier, d'initialiser nb_lignes, nb_cols et de construire terrain.
void lecture_mnt(char* nom);

// La fonction pour calculer les directions est déjà implémentée
// Elle prend en paramètre un terrain (paramètre data) et elle renvoie les directions dans le tableau dir.
// Attention data est de taille (n_l+2) x n_c afin de prendre en compte les ghosts.
// Ce n'est pas nécessaire en séquentiel mais cette fonction sera disponible également pour votre parallélisation.
// dir est de taille n_l x n_c
void calcul_direction(float *data, int *dir, int n_l, int n_c);
int f_bord1(float ref, float* tab, float no_value);
int f_bord2(float ref, float* tab, float no_value);
int f(float ref, float* tab, float no_value);
// la fonction conversion est une proposition pour convertir les directions
// en une couleur pour visualiser les directions calculées
int conversion(int val);
// la fonction conversion_acc est une proposition pour convertir les flots d'accumulation
// en une couleur
int conversion_acc(int val);
// la fonction conversion_bassin est une proposition pour convertir le n° d'un bassin par une couleur.
int conversion_bassin(int val);
// Les fonctions pour échanger les ghosts (une fonction pour des floats et une pour les int)
void echange_ghosts(float* data, int n_l, int n_c);
void echange_ghosts_int(int* data, int n_l, int n_c);

// Les 2 fonctions pour le calcul des flots d'accumulation
// calcul_accumulation réalise une itération du calcul et
// renvoie une valeur indiquant si des cellules ont été mises
// et donc s'il faut poursuivre les itérations.
// f_acc calcule le flot d'accumulation d'une cellule c à partir
// de tab qui stocke le voisinage suivi des directions impliquées
// dir_bord est un tableau permettant de tester si une cellule
// du voisinage se déverse dans c.

unsigned calcul_accumulation(int *dir, int *acc, int n_l, int n_c);
int f_acc(int* tab, int no_value, int* dir_bord, int l);

// Pour le bassin versant
void calcul_bassin(int *bassin, int *dir, int n_l, int n_c);
int f_bassin(int* bassin, int *dir, int n_l, int n_c, int i, int j);

static int num = 100;

////////////////////////////////////
// Les trois fonctions EasyPAP pour la partie séquentielle
void mnt_init (void)
{
  PRINT_DEBUG ('u', "Image size is %dx%d\n", DIM, DIM);
  PRINT_DEBUG ('u', "Block size is %dx%d\n", TILE_W, TILE_H);
  PRINT_DEBUG ('u', "Press <SPACE> to pause/unpause, <ESC> to quit.\n");
}

int mnt_do_tile_default (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++)
      cur_img (i, j) = conversion(direction[(i+1)*DIM+j]);
  return 0;
}
int mnt_do_tile_accumulation (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++)
      cur_img (i, j) = conversion_acc(accumulation[(i+1)*DIM+j]);
  return 0;
}

int mnt_do_tile_bassin (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++)
      cur_img (i, j) = conversion_bassin(bassin[i*DIM+j]);
  return 0;
}

// Attention afin de ne pas avoir trop de cas particulier
// La version séquentielle utilise les fonctions proposées
// pour la version paralléle et donc travaille sur des matrices
// qui ont deux lignes supplémentaires (comme si on gérait déjà des
// ghosts.
unsigned mnt_compute_seq(unsigned nb_iter) {
    for (unsigned it = 1; it <= nb_iter; it++) {
        lecture_mnt(filename);

        calcul_direction(terrain, direction + nb_cols, nb_lignes, nb_cols);

        // Pour utiliser une fonction disponible pour la version parallèle
        // il y a deux lignes supplémentaires dans direction
        // la première est mise à 1 pour indiquer que la cellule correspondante
        // se déverse vers le haut
        // la dernière est mise à 5 pour indiquer que la cellule correspondante
        // se déverse vers le bas
        for (int i = 0; i < nb_cols; i++) {
            direction[i] = 1;
            direction[(nb_lignes + 1) * nb_cols + i] = 5;
        }

        // Pour les flots d'accumulations les 2 lignes supplémentaires sont
        // mises à 0. Mais via la convention de la matrice direction
        // on n'utilisera pas ces lignes dans le calcul.
        for (int i = 0; i < nb_cols; i++) {
            accumulation[i] = 0;
            accumulation[(nb_lignes + 1) * nb_cols + i] = 0;
        }
        // Le reste de la matrice est initialisée à -1 pour non marqué.
        for (int i = 0; i < nb_lignes; i++)
            for (int j = 0; j < nb_cols; j++)
                accumulation[(i + 1) * nb_cols + j] = -1;
        int stop = 0;
        while (stop != 1) {
            stop = calcul_accumulation(direction, accumulation, nb_lignes, nb_cols);
        }

	 bassin = (int*) malloc(nb_lignes*nb_cols* sizeof(int));

        for (int i=0; i<nb_lignes; i++)
            for (int j=0; j<nb_cols; j++)
                bassin[i*nb_cols+j] = -1;

        calcul_bassin(bassin,direction+nb_cols,nb_lignes,nb_cols);

        // Pour les images :
        // si on utilise la version par défaut on aura l'image relative aux directions
        // si on utilise la version mnt_do_tile_accumulation (option -wt accumulation dans le run)
        // on aura l'image relative aux accumulations.
        do_tile(0, 0, DIM, DIM, 0);
    }
    return 0;
}

////////////////////////////////////
// Les trois fonctions EasyPAP pour la partie parallèle en mpi
// dont les deux versions pour do_tile

static int mpi_rank = -1;
static int mpi_size = -1;

static float *terrain_local;
static int *dir_local;
static int *acc_local;
static int *bassin_local;

static int nb_lignes_local = -1;

// Les deux fonctions suivantes ne sont utiles que pour générer une image
// pour les utiliser il faut penser à l'option -wt dans la ligne de commande
// pour indiquer la version utilisée : mpi ou mpi_acc

int mnt_do_tile_mpi(int x, int y, int width, int height) {
    for (int i = y; i < y + height; i++)
        for (int j = x; j < x + width; j++) {
            cur_img(i, j) = conversion(dir_local[(i - y + 1) * nb_cols + (j - x)]);
        }
    return 0;
}

int mnt_do_tile_mpi_acc(int x, int y, int width, int height) {
    for (int i = y; i < y + height; i++)
        for (int j = x; j < x + width; j++) {
            cur_img(i, j) = conversion_acc(acc_local[(i - y + 1) * nb_cols + (j - x)]);
        }
    return 0;
}

int mnt_do_tile_mpi_bassin(int x, int y, int width, int height) {
    for (int i = y; i < y + height; i++)
        for (int j = x; j < x + width; j++) {
            cur_img(i, j) = conversion_bassin(bassin_local[(i - y + 1) * nb_cols + (j - x)]);
        }
    return 0;
}

void mnt_init_mpi(void) {
    easypap_check_mpi();
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
}

unsigned mnt_compute_mpi(unsigned nb_iter) {
    for (unsigned it = 1; it <= nb_iter; it++) {
        if (mpi_rank == 0)
            lecture_mnt(filename);

        MPI_Bcast(&nb_lignes, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&nb_cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&no_value, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);

        nb_lignes_local = nb_lignes / mpi_size;

        terrain_local = (float *) malloc((nb_lignes_local + 2) * nb_cols * sizeof(float));

        if (mpi_rank == 0)
            for (int i = 0; i < nb_cols; i++)
                terrain_local[i] = no_value;
        if (mpi_rank == mpi_size - 1)
            for (int i = 0; i < nb_cols; i++)
                terrain_local[(nb_lignes_local + 1) * nb_cols + i] = no_value;

        MPI_Scatter(terrain + nb_cols, nb_lignes_local * nb_cols, MPI_FLOAT, terrain_local + nb_cols,
                    nb_lignes_local * nb_cols, MPI_FLOAT, 0, MPI_COMM_WORLD);

        echange_ghosts(terrain_local, nb_lignes_local, nb_cols);

        dir_local = (int *) malloc((nb_lignes_local + 2) * nb_cols * sizeof(int));
        acc_local = (int *) malloc((nb_lignes_local + 2) * nb_cols * sizeof(int));
        calcul_direction(terrain_local, dir_local + nb_cols, nb_lignes_local, nb_cols);

        // Passage aux flots d'accumulation
        // Il faut mettre à jour les directions pour avoir les bonnes infos
        echange_ghosts_int(dir_local, nb_lignes_local, nb_cols);

        if (mpi_rank == 0)
            for (int i = 0; i < nb_cols; i++)
                dir_local[i] = 1;

        if (mpi_rank == mpi_size - 1)
            for (int i = 0; i < nb_cols; i++)
                dir_local[(nb_lignes_local + 1) * nb_cols + i] = 5;

        for (int i = 0; i < nb_cols; i++) {
            acc_local[i] = -1;
            acc_local[(nb_lignes_local + 1) * nb_cols + i] = -1;
        }

        for (int i = 0; i < nb_lignes_local; i++)
            for (int j = 0; j < nb_cols; j++)
                acc_local[(i + 1) * nb_cols + j] = -1;

        int stop = 0;

        while (stop!=mpi_size) {
            int stop_local = calcul_accumulation(dir_local, acc_local, nb_lignes_local, nb_cols);
            MPI_Allreduce(&stop_local, &stop, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
            if (stop != mpi_size)
                echange_ghosts_int(acc_local,nb_lignes_local,nb_cols);
        }


        bassin_local = (int *) malloc((nb_lignes_local + 2) * nb_cols * sizeof(int));

        for (int i = 0; i < (nb_lignes_local+2)*nb_cols; i++)
          bassin_local[i] = -1;

        if (mpi_rank == 0){
          for (int i = 0; i < nb_cols; ++i){
            bassin_local[i] = no_bassin_value;
          }
        }

        if (mpi_rank == mpi_size-1){
          for (int i = 0; i < nb_cols; ++i){
            bassin_local[(nb_lignes_local+1) * nb_cols + i] = no_bassin_value;
          }
        }

        int nb_puits_local = 0;
        for (int i = nb_cols; i < (nb_lignes_local+1)*nb_cols; ++i){
          if (dir_local[i] == 0) nb_puits_local++;
        } 

        int *nb_puits = (int *) malloc(mpi_size * sizeof(int));
        MPI_Allgather(&nb_puits_local, 1, MPI_INT, nb_puits, 1, MPI_INT, MPI_COMM_WORLD);

        for (int i = 0; i < mpi_rank; ++i){
          num += nb_puits[i];
        }

        stop = 0;

        while (stop!=mpi_size) {
            calcul_bassin(bassin_local+nb_cols, dir_local+nb_cols, nb_lignes_local, nb_cols);
            int stop_local = 1;
            for (int i = nb_cols; i < (nb_lignes_local+1)*nb_cols; ++i){
              if (bassin_local[i] == -1) stop_local = 0;
            }
            MPI_Allreduce(&stop_local, &stop, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
            if (stop != mpi_size)
                echange_ghosts_int(bassin_local,nb_lignes_local,nb_cols);
        }
        bassin = (int *) malloc(nb_lignes*nb_cols*sizeof(int));
        MPI_Gather(bassin_local + nb_cols, nb_lignes_local*nb_cols, MPI_INT, bassin, nb_lignes_local*nb_cols, MPI_INT, 0, MPI_COMM_WORLD);

        /*if (mpi_rank == 0){
          printf("\n");
            printf("pid = %d\n",mpi_rank);
            for (int i = 0; i < (nb_lignes); ++i){
              for (int j = 0; j < nb_cols; ++j){
                printf("%4.1d",bassin[i * nb_cols + j]);
              }
              printf("\n");
          }
        }*/

          // Première version pour la génération de l'image
        // avec l'option -wt mpi ou -wt mpi_acc pour respectivement la direction et les flots d'accumulation
        do_tile(0, mpi_rank * nb_lignes_local, nb_cols, nb_lignes_local, mpi_rank);

        MPI_Gather(image + mpi_rank * nb_lignes_local * DIM, nb_lignes_local * DIM, MPI_INT, image,
                   nb_lignes_local * DIM, MPI_INT, 0, MPI_COMM_WORLD);



    }
    return 0;
}
////////////////////////////////////
// Pour la version OpenMP
unsigned mnt_compute_omp(unsigned nb_iter) {
    for (unsigned it = 1; it <= nb_iter; it++) {
        lecture_mnt(filename);

#pragma omp parallel
        {
            printf("je suis %d ", omp_get_thread_num());
        }
    }
    return 0;
}

/////////////////////////////////////////////////////////////////

void lecture_mnt(char *nom) {
    FILE *f = fopen(nom, "r");
    if (f != NULL) {
        int tmp;
        if (fscanf(f, "%d", &tmp) == 1);
        nb_lignes = tmp;
        if (fscanf(f, "%d", &tmp) == 1);
        nb_cols = tmp;
        if (fscanf(f, "%d", &tmp) == 1);
        if (fscanf(f, "%d", &tmp) == 1);
        if (fscanf(f, "%d", &tmp) == 1);
        if (fscanf(f, "%f", &no_value) == 1);


        terrain = (float *) calloc((2 + nb_lignes) * nb_cols, sizeof(float));
        direction = (int *) malloc((2 + nb_lignes) * nb_cols * sizeof(float));
        accumulation = (int *) malloc((2 + nb_lignes) * nb_cols * sizeof(float));

        for (int j = 0; j < nb_cols; j++) {
            terrain[j] = no_value;
            terrain[(nb_lignes + 1) * nb_cols + j] = no_value;
        }

        for (int i = 0; i < nb_lignes; i++)
            for (int j = 0; j < nb_cols; j++)
                if (fscanf(f, "%f", &(terrain[(i + 1) * nb_cols + j])) == 1);
    }

    for (int j = 0; j < nb_cols; j++) {
        terrain[j] = no_value;
        terrain[(nb_lignes + 1) * nb_cols + j] = no_value;
    }
}

int f_bord1(float ref, float *tab, float no_value) {
    float min = ref;
    int code = 0;
    for (int i = 0; i < 5; i++)
        if (tab[i] != no_value) {
            if (tab[i] < min) {
                min = tab[i];
                code = i + 1;
            }
        }
    return code;
}

int f_bord2(float ref, float *tab, float no_value) {
    float min = ref;
    int code = -1;
    for (int i = 0; i < 5; i++)
        if (tab[i] != no_value) {
            if (tab[i] < min) {
                min = tab[i];
                code = i;
            }
        }
    if (code == -1)
        return 0;
    else if (code == 4)
        return 1;
    else
        return code + 5;
}

int f(float ref, float *tab, float no_value) {

    float min = ref;
    int code = 0;
    for (int i = 0; i < 8; i++)
        if (tab[i] != no_value) {
            if (tab[i] < min) {
                min = tab[i];
                code = i + 1;
            }
        }
    return code;
}

void calcul_direction(float *data, int *dir, int n_l, int n_c) {
    int x, y;
    int x1, y1;
    int x2, y2;
    float tab[8];
    float tab_bord[5];
    for (int i = 0; i < n_l; i++) {
        x = i + 1;
        x1 = x - 1;
        x2 = x + 1;
        for (int j = 0; j < n_c; j++) {
            y = j;
            y1 = y - 1;
            y2 = y + 1;
            float val = data[x * n_c + y];
            if (val != no_value) {
                if (j == 0) {
                    tab_bord[0] = data[x1 * n_c + y];
                    tab_bord[1] = data[x1 * n_c + y2];
                    tab_bord[2] = data[x * n_c + y2];
                    tab_bord[3] = data[x2 * n_c + y2];
                    tab_bord[4] = data[x2 * n_c + y];
                    dir[i * n_c + j] = f_bord1(val, tab_bord, no_value);
                } else if (j == (n_c - 1)) {
                    tab_bord[0] = data[x2 * n_c + y];
                    tab_bord[1] = data[x2 * n_c + y1];
                    tab_bord[2] = data[x * n_c + y1];
                    tab_bord[3] = data[x1 * n_c + y1];
                    tab_bord[4] = data[x1 * n_c + y];
                    dir[i * n_c + j] = f_bord2(val, tab_bord, no_value);
                } else {
                    tab[0] = data[x1 * n_c + y];
                    tab[1] = data[x1 * n_c + y2];
                    tab[2] = data[x * n_c + y2];
                    tab[3] = data[x2 * n_c + y2];
                    tab[4] = data[x2 * n_c + y];
                    tab[5] = data[x2 * n_c + y1];
                    tab[6] = data[x * n_c + y1];
                    tab[7] = data[x1 * n_c + y1];
                    dir[i * n_c + j] = f(val, tab, no_value);
                }
            } else {
                dir[i * n_c + j] = no_dir_value;
            }
        }
    }
}

int f_acc(int *tab, int no_value, int *dir_bord, int l) {
    int val = 0;
    int nb1 = 0;
    int nb2 = 0;

    for (int i = 0; i < l; i++) {
        if (tab[l + i] != no_value){
            if (tab[l + i] == dir_bord[i]) {
                nb1++;
                if (tab[i] != -1) {
                    val += tab[i];
                    nb2++;
                }
            }
        }
    }
    if (nb1 == nb2)
        val++;
    else
        val = -1;
    return val;
}

unsigned calcul_accumulation(int *dir, int *acc, int n_l, int n_c) {
    unsigned stop = 0;
    int nb_non_calculs = 0;
    int x, y;
    int x1, y1;
    int x2, y2;
    int tab[16];
    int tab_bord[10];
    for (int i = 0; i < n_l; i++) {
        x = i + 1;
        x1 = x - 1;
        x2 = x + 1;
        for (int j = 0; j < n_c; j++) {
            y = j;
            y1 = y - 1;
            y2 = y + 1;
            int d = dir[x * n_c + y];
            if (d != no_dir_value) {
                if (acc[x * n_c + j] == -1) {
                    if (j == 0) {
                        tab_bord[0] = acc[x1 * n_c + y];
                        tab_bord[1] = acc[x1 * n_c + y2];
                        tab_bord[2] = acc[x * n_c + y2];
                        tab_bord[3] = acc[x2 * n_c + y2];
                        tab_bord[4] = acc[x2 * n_c + y];
                        tab_bord[5] = dir[x1 * n_c + y];
                        tab_bord[6] = dir[x1 * n_c + y2];
                        tab_bord[7] = dir[x * n_c + y2];
                        tab_bord[8] = dir[x2 * n_c + y2];
                        tab_bord[9] = dir[x2 * n_c + y];
                        int res = f_acc(tab_bord, no_dir_value, dir_bord1, 5);
                        if (res != -1)
                            acc[x * n_c + j] = res;
                        else {
                            nb_non_calculs++;
                        }
                    } else if (j == (n_c - 1)) {
                        tab_bord[0] = acc[x2 * n_c + y];
                        tab_bord[1] = acc[x2 * n_c + y1];
                        tab_bord[2] = acc[x * n_c + y1];
                        tab_bord[3] = acc[x1 * n_c + y1];
                        tab_bord[4] = acc[x1 * n_c + y];
                        tab_bord[5] = dir[x2 * n_c + y];
                        tab_bord[6] = dir[x2 * n_c + y1];
                        tab_bord[7] = dir[x * n_c + y1];
                        tab_bord[8] = dir[x1 * n_c + y1];
                        tab_bord[9] = dir[x1 * n_c + y];
                        int res = f_acc(tab_bord, no_dir_value, dir_bord2, 5);
                        if (res != -1)
                            acc[x * n_c + j] = res;
                        else {
                            nb_non_calculs++;
                        }
                    } else {
                        tab[0] = acc[x1 * n_c + y];
                        tab[1] = acc[x1 * n_c + y2];
                        tab[2] = acc[x * n_c + y2];
                        tab[3] = acc[x2 * n_c + y2];
                        tab[4] = acc[x2 * n_c + y];
                        tab[5] = acc[x2 * n_c + y1];
                        tab[6] = acc[x * n_c + y1];
                        tab[7] = acc[x1 * n_c + y1];
                        tab[8] = dir[x1 * n_c + y];
                        tab[9] = dir[x1 * n_c + y2];
                        tab[10] = dir[x * n_c + y2];
                        tab[11] = dir[x2 * n_c + y2];
                        tab[12] = dir[x2 * n_c + y];
                        tab[13] = dir[x2 * n_c + y1];
                        tab[14] = dir[x * n_c + y1];
                        tab[15] = dir[x1 * n_c + y1];
                        int res = f_acc(tab, no_dir_value, dir_general, 8);
                        if (res != -1)
                            acc[x * n_c + j] = res;
                        else
                            nb_non_calculs++;
                    }
                }
            }
        }
    }
    if (nb_non_calculs == 0)
        stop = 1;
    return stop;
}

void calcul_bassin(int *bassin, int *dir, int n_l, int n_c) {
    for (int i = 0; i < n_l; i++)
        for (int j = 0; j < n_c; j++) {
            if (dir[i * n_l + j] != no_dir_value) {
                if (bassin[i * n_c + j] == -1)
                    bassin[i * n_c + j] = f_bassin(bassin, dir, n_l, n_c, i, j);
            }
            else {
                bassin[i*n_c+j] = no_bassin_value;
            }
        }
}

int f_bassin(int *bassin, int *dir, int n_l, int n_c, int i, int j) {
  //Condition pour ne pas regarder des valeurs en dehors du tableau (ghosts compris)
  if (i >= -1 && i <= n_l) {
    //S'il y a un puis sur un ghost on récupère la valeur
    if (i == -1 && dir[i*n_c + j] == 0){
      return bassin[i*n_c+j];
    }
    if (i == n_l && dir[i*n_c+j] == 0){
      return bassin[i*n_c+j];
    }


    if (dir[i * n_c + j] == 0) {
      if (bassin[i * n_c + j] == -1) {
        num++;
        bassin[i * n_c + j] = num;
        return num;
      }
      else { 
        return bassin[i * n_c +j];
      }
    } else if (bassin[i * n_c + j] != -1) {
      return bassin[i * n_c + j];
    }
    else {
      int etiquette;
      switch (dir[i * n_c + j]) {
      case 1:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i - 1, j);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;
      case 2:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i - 1, j + 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;	    
      case 3:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i, j + 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;
      case 4:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i + 1, j + 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;	    
      case 5:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i + 1, j);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;
      case 6:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i + 1, j - 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;	    
      case 7:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i, j - 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;
      case 8:
        etiquette = f_bassin(bassin, dir, n_l, n_c, i - 1, j - 1);
        bassin[ i * n_c + j] = etiquette;
        return etiquette;
      }
    }
    return 0;
  }
  return -1;
}

// La fonction pour les ghosts
void echange_ghosts(float* data, int n_l, int n_c){
    MPI_Request request_send;
    MPI_Request request_recv;
    if (mpi_rank!=mpi_size-1)
        MPI_Isend(data+n_c*n_l,n_c,MPI_FLOAT,mpi_rank+1,10,MPI_COMM_WORLD,&request_send);
    if (mpi_rank!=0) {
        MPI_Irecv(data, n_c, MPI_FLOAT, mpi_rank - 1, 10, MPI_COMM_WORLD, &request_recv);
        MPI_Wait(&request_recv,MPI_STATUS_IGNORE);
    }
    if (mpi_rank!=0) {
        MPI_Isend(data + n_c, n_c, MPI_FLOAT, mpi_rank - 1, 10, MPI_COMM_WORLD, &request_send);
    }
    if (mpi_rank!=mpi_size-1) {
        MPI_Irecv(data+n_c*(n_l+1), n_c, MPI_FLOAT, mpi_rank+1, 10, MPI_COMM_WORLD, &request_recv);
        MPI_Wait(&request_recv,MPI_STATUS_IGNORE);
    }


}

void echange_ghosts_int(int* data, int n_l, int n_c){
    MPI_Request request_send;
    MPI_Request request_recv;
    if (mpi_rank!=mpi_size-1)
        MPI_Isend(data+n_c*n_l,n_c,MPI_INT,mpi_rank+1,10,MPI_COMM_WORLD,&request_send);
    if (mpi_rank!=0) {
        MPI_Irecv(data, n_c, MPI_INT, mpi_rank - 1, 10, MPI_COMM_WORLD, &request_recv);
        MPI_Wait(&request_recv,MPI_STATUS_IGNORE);
    }
    if (mpi_rank!=0) {
        MPI_Isend(data + n_c, n_c, MPI_INT, mpi_rank - 1, 10, MPI_COMM_WORLD, &request_send);
    }
    if (mpi_rank!=mpi_size-1) {
        MPI_Irecv(data+n_c*(n_l+1), n_c, MPI_INT, mpi_rank+1, 10, MPI_COMM_WORLD, &request_recv);
        MPI_Wait(&request_recv,MPI_STATUS_IGNORE);
    }
}

// Cette fonction permet juste de convertir le code des directions par des couleurs.

int conversion(int val) {
  int color = rgba(255,255,255,255);
  switch (val) {
  case 0:
    color = rgba(255,255,255,255);
    break;
  case 1:
    color =  rgba(255,0,0,255);
    break;
  case 2:
    color =  rgba(255,127,0,255);
    break;
  case 3:
    color =  rgba(255,255,0,255);
    break;
  case 4:
    color =  rgba(127,255,0,255);
    break;
  case 5:
    color =  rgba(0,127,0,255);
    break;
  case 6:
    color =  rgba(0,0,255,255);
    break;
  case 7:    	
    color =  rgba(0,255,255,255);
    break;
  case 8:
    color =  rgba(0,0,0,255);
    break;
  default:
    color =  rgba(255,255,255,255);
  }

  return color;
}

int conversion_acc(int val) {

  int color;
  if (val==-1)
    color = rgba(0,0,0,255);
  else {
    float test =  ((float)val * 255.0) / (float)(nb_cols*nb_lignes);
    if ((int)(test)*cst >255)
      color = rgba(255,0,0,255);
    else
      color = rgba((int)(test*cst),0,0,255); 
  }
  return color;
}
int conversion_bassin(int val) {
    static unsigned couleur0[4] = {0,127,0,255};
    static unsigned couleur1[4] = {255,0,0,255};
    static unsigned couleur2[4] = {127,255,0,255};
    static unsigned couleur3[4] = {255,127,0,255};
    static unsigned couleur4[4] = {127,255,127,255};
    static unsigned couleur5[4] = {0,0,255,255};
    static unsigned couleur6[4] = {0,255,127,255};
    static unsigned couleur7[4] = {255,255,255,255};
    static unsigned couleur8[4] = {127,127,127,255};
    static unsigned couleur9[4]= {0,255,0,255};
    int v = val%10;

    if (val ==-10)
      return rgba(0,0,0,255);
    else {
      switch (v) {
      case 0:
	return rgba(couleur0[0], couleur0[1], couleur0[2], couleur0[3]);
      case 1:
	return rgba(couleur1[0], couleur1[1], couleur1[2], couleur1[3]);
      case 2:
	return rgba(couleur2[0], couleur2[1], couleur2[2], couleur2[3]);
      case 3:
	return rgba(couleur3[0], couleur3[1], couleur3[2], couleur3[3]);
      case 4:
	return rgba(couleur4[0], couleur4[1], couleur4[2], couleur4[3]);
      case 5:
	return rgba(couleur5[0], couleur5[1], couleur5[2], couleur5[3]);
      case 6:
	return rgba(couleur6[0], couleur6[1], couleur6[2], couleur6[3]);
      case 7:
	return rgba(couleur7[0], couleur7[1], couleur7[2], couleur7[3]);
      case 8:
	return rgba(couleur8[0], couleur8[1], couleur8[2], couleur8[3]);
      case 9:
	return rgba(couleur9[0], couleur9[1], couleur9[2], couleur9[3]);
      }
    }
    return rgba(50,50,50,255);
}
