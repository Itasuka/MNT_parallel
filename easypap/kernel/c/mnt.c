
#include "easypap.h"

#include <math.h>
#include <omp.h>


// Les données disponibles sont les suivantes
// Attention lors du lancement à bien signaler la taille pour que l'image générée soit correcte
// Par exemple pour la dalle grd il faudra ajouter -s 1025 à votre ligne de lancement
//static char* filename = "data/mnt/petit.txt"; // ce fichier correspond à l'exemple de la fiche ipynb 6x6
static char* filename = "data/mnt/jeu_essai.txt"; // ce fichier correspond à un extrait 32 x 32 de la dalle grd
//static char* filename = "data/mnt/grd_618360_6754408.txt"; // ce fichier correspond à la dalle grd complète 1025x1025
//static char* filename = "data/mnt/alpes.txt"; // ce fichier correspond aux données sur les alpes 1024x1024

static float no_value;
static int nb_lignes;
static int nb_cols;

// Les tableaux pour construire le terrain à partir du mnt et calculer les directions.
// Attention terrain est défini avec 2 lignes supplémentaires pour permettre des ghosts 
static float* terrain;
static int* direction;
static int* flot;

// Cette fonction permet de lire le fichier, d'initialiser nb_lignes, nb_cols et de construire terrain.
void lecture_mnt(char* nom);

//cst
static int cst = 10000;

// La fonction pour calculer les directions est déjà implémentée
// Elle prend en paramètre un terrain (paramètre data) et elle renvoie les directions dans le tableau dir.
// Attention data est de taille (n_l+2) x n_c afin de prendre en compte les ghosts.
// Ce n'est pas nécessaire en séquentiel mais cette fonction sera disponible également pour votre parallélisation.
// dir est de taille n_l x n_c
void calcul_direction(float *data, int *dir, int n_l, int n_c);

// Les 3 fonctions ci-dessous permettent de calculer la direction en fonction
// d'un voisinage avec les 2 cas relatifs au bord droit et gauche
// et le cas général
int f_bord1(float ref, float* tab, float no_value);
int f_bord2(float ref, float* tab, float no_value);
int f(float ref, float* tab, float no_value);

// Une fonction de conversion pour construire une image
int conversion(int val);
int conversion_acc(int val);

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
      cur_img (i, j) = conversion_acc(flot[i*DIM+j]);
  return 0;
}


unsigned mnt_compute_seq (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {
    lecture_mnt(filename);
    direction = (int*)  malloc((nb_lignes*nb_cols)*sizeof(float));
    calcul_direction(terrain,direction,nb_lignes,nb_cols);


    /*
     * int inv_direction[3][3] = [{4,5,6},
     * 				  {3,-1,7},
     * 				  {2,1,8}]
     * for vx de -1 à 1 faire
     * 	for vy de -1 à 1 faire
     * 		vérifier que 0 <= i + vx < nb_lignes et 0 <= j+vy < nb_cols
     * 		if direction[(i+vx) * nb_cols + (j+vy)] = inv_direction[vx+1][vy+1];
     */

    flot = (int*) malloc((nb_lignes*nb_cols)*sizeof(float)); 
    for (int i = 0; i < nb_lignes*nb_cols; ++i){
	    flot[i] = -1;
    }
    bool flag = true;

    /*for (int i = 0; i < nb_lignes; ++i){
	    printf("ligne %d - ",i);
	    for (int j = 0; j < nb_cols; ++j){
		    printf("%d ",direction[i*nb_cols+j]);
	    }
	    printf("\n");
    }*/


    while (flag){
/*	    for (int i = 0; i < nb_lignes; ++i){
		    for (int j = 0; j < nb_cols; ++j){
			    if (flot[i * nb_cols + j] == -1) printf("i = %d et j= %d et direction = %d\n",i,j,direction[i*nb_cols+j]);
		    }
	    }
*/	    //printf("\n");
	    for (int i = 0; i < nb_lignes; ++i){
		    for (int j = 0 ; j < nb_cols; ++j){
			   int fno = -1; int fn = -1; int fne = -1; int fo = -1; int fe = -1; int fso = -1; int fs = -1; int fse = -1;
			   int dno = -1; int dn = -1; int dne = -1; int ddo = -1; int de = -1; int dso = -1; int ds = -1; int dse = -1;
			   bool bn = false; bool bo = false; bool be = false; bool bs = false;
			   if (i > 0) bn = true;
			   if (i < nb_lignes-1) bs = true;
			   if (j > 0) bo = true;
			   if (j < nb_cols-1) be = true;

			   int res = 1;

			   if (bn || bo || be || bs){
				   if (bn && bo){ fno = flot[i * nb_cols - nb_cols + j -1]; dno = direction[i * nb_cols - nb_cols + j -1];}
				   if (bn){ fn = flot[i * nb_cols - nb_cols + j]; dn = direction[i * nb_cols - nb_cols + j];}
				   if (bn && be){ fne = flot[i * nb_cols - nb_cols + j +1]; dne = direction[i * nb_cols - nb_cols + j +1];}
				   if (bo){ fo = flot[i * nb_cols + j -1]; ddo = direction[i * nb_cols + j -1];}
				   if (be){ fe = flot[i * nb_cols + j +1]; de = direction[i * nb_cols + j +1];}
				   if (bs && bo){ fso = flot[i * nb_cols + nb_cols + j -1]; dso = direction[i * nb_cols + nb_cols + j -1];}
				   if (bs){ fs = flot[i * nb_cols + nb_cols + j ]; ds = direction[i * nb_cols + nb_cols + j ];}
				   if (bs && be){ fse = flot[i * nb_cols + nb_cols + j +1]; dse = direction[i * nb_cols + nb_cols + j +1];}

				   if (dno == 4){ if (fno > -1) res += fno; else continue;}
				   if (dn == 5){ if (fn > -1) res += fn; else continue;}
				   if (dne == 6){ if (fne > -1) res += fne; else continue;}
				   if (ddo == 3){ if (fo > -1) res += fo; else continue;}
           if (de == 7){ if (fe > -1) res += fe; else continue;}
				   if (dso == 2){ if (fso > -1) res += fso; else continue;}
				   if (ds == 1){ if (fs > -1) res += fs; else continue;}
				   if (dse == 8){ if (fse > -1) res += fse; else continue;}
			   }
			   flot[i * nb_cols + j] = res;
		    }
	    }
	    flag = false;
	    for (int i = 0; i < nb_lignes*nb_cols; ++i){
			    flag = flag || (flot[i] == -1);
	    }
    }

    /*for (int i = 0; i < nb_lignes; ++i){
	    for (int j = 0; j < nb_cols; ++j){
		    printf("%4.1d ",flot[i*nb_cols+j]);
	    }
	    printf("\n");
    }*/

    do_tile(0,0,DIM,DIM,0);
  }

  return 0;
}

////////////////////////////////////
// Les deux fonctions EasyPAP pour la partie séquentielle
// la fonction mnt_do_tile_default ne change pas.
static int mpi_rank = -1;
static int mpi_size = -1;

static float* terrain_local;
static int* dir_local;
static int* flot_local;

static int nb_lignes_local = -1;

//Do tile permettant d'afficher la zone de travail de chaque processus
int mnt_do_tile_mpi (int x, int y, int width, int height)
{
  for (int i = y; i < y + height; i++)
    for (int j = x; j < x + width; j++)
      cur_img (i, j) = conversion_acc(flot_local[(i-y+1)*width+(j-x)]);
  return 0;
}

void mnt_init_mpi(void)
{
  easypap_check_mpi();
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);   
}
unsigned mnt_compute_mpi(unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {
    if (mpi_rank==0)
      lecture_mnt(filename);
    // A compléter à partir d'ici
    
    // 1. initialisation des différentes valeurs (taille etc)
    MPI_Bcast(&nb_lignes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&nb_cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
    nb_lignes_local = (nb_lignes/mpi_size);
    //Taille des tableau + 2 pour prendre en compte les ghosts
    terrain_local = (float *) malloc((nb_lignes_local+2) * nb_cols * sizeof(float));
    dir_local = (int *) malloc((nb_lignes_local+2) * nb_cols * nb_cols*sizeof(int));
    flot_local = (int *) calloc((nb_lignes_local+2) * nb_cols,sizeof(int));

    // 2. La distribution du terrain en tenant compte des ghosts
    MPI_Scatter(terrain+nb_cols, nb_lignes_local*nb_cols, MPI_FLOAT, terrain_local+nb_cols, nb_lignes_local*nb_cols, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // 3. Les échanges de ghosts
    MPI_Request request;
    //Utilisation de réception bloquante pour s'assurer d'avoir reçu les différentes valeurs avant de calculer les directions
    //Envoie non bloquant de la première ligne de valeurs et Reception bloquante de la ligne de ghost inférieure
    MPI_Isend(terrain_local+nb_cols, nb_cols, MPI_FLOAT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, &request);
    MPI_Recv(terrain_local+(nb_lignes_local+1)*nb_cols, nb_cols, MPI_FLOAT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    //Envoie non bloquant de la dernière ligne de valeurs et Reception bloquante de la ligne de ghost supérieure
    MPI_Isend(terrain_local+nb_cols*nb_lignes_local, nb_cols, MPI_FLOAT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, &request);
    MPI_Recv(terrain_local, nb_cols, MPI_FLOAT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    //Mise en place des no_values pour la première ligne de ghost du processus 0 (chargé du haut de l'image)
    if (mpi_rank == 0){
      for (int i = 0; i < nb_cols; ++i){
        terrain_local[i] = no_value;
      }
      MPI_Send(&no_value, 1, MPI_FLOAT, mpi_size-1, 0, MPI_COMM_WORLD);
    }
    //Mise en place des no_values pour la dernière ligne de ghost du processus mpi_size-1 (chargé du bas de l'image)
    if (mpi_rank == mpi_size-1){
      MPI_Recv(&no_value, 1, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      for (int i = 0; i < nb_cols; ++i){
        terrain_local[(nb_lignes_local+1)*nb_cols + i] = no_value;
      }
    }
    // 4. Le calcul des directions.
    calcul_direction(terrain_local, dir_local+nb_cols, nb_lignes_local, nb_cols);

    //Envoie des ghosts des directions
    //Utilisation de réception bloquante pour s'assurer d'avoir reçu les différentes valeurs avant de calculer les directions
    //Envoie non bloquant de la première ligne de valeurs et Reception bloquante de la ligne de ghost inférieure
    MPI_Isend(dir_local+nb_cols, nb_cols, MPI_FLOAT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, &request);
    MPI_Recv(dir_local+(nb_lignes_local+1)*nb_cols, nb_cols, MPI_FLOAT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    //Envoie non bloquant de la dernière ligne de valeurs et Reception bloquante de la ligne de ghost supérieure
    MPI_Isend(dir_local+nb_cols*nb_lignes_local, nb_cols, MPI_FLOAT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, &request);
    MPI_Recv(dir_local, nb_cols, MPI_FLOAT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Mise en place de valeurs de directions n'affectant pas le futur calcul des flots pour la première ligne
    if (mpi_rank == 0){
      for (int i = 0; i < nb_cols; ++i){
        dir_local[i] = -1;
      }
    }
    // Mise en place de valeurs de directions n'affectant pas le futur calcul des flots pour la dernière ligne
    if (mpi_rank == mpi_size-1){
      for (int i = 0; i < nb_cols; ++i){
        dir_local[(nb_lignes_local+1)*nb_cols + i] = -1;
      }
    }

    //Affichage du tableau de directions local
    /*printf("Mon pid %d\n",mpi_rank);
    for (int i = 0; i < nb_lignes_local + 2; ++i){
      for (int j = 0; j < nb_cols; ++j){
        printf("%d ", dir_local[i*nb_cols + j]);
      } 
      printf("\n");
    }*/

    //Création du tableau de directions globales pour le processus 0
    if (mpi_rank == 0)
      direction = (int *) calloc(nb_lignes*nb_cols,sizeof(int));
    //Rassemblement des directions locales dans le tableau de directions globales sur le processus 0
    //MPI_Gather(dir_local + nb_cols, nb_lignes_local*nb_cols, MPI_INT, direction, nb_lignes_local*nb_cols, MPI_INT, 0, MPI_COMM_WORLD);  

    //Initialisation des flots locaux à -1
    for (int i = 0; i < (nb_lignes_local+2)*nb_cols; ++i){
      flot_local[i] = -1;
    }

    //Tableau des directions inverses 
    int inv_direction[3][3] = 
             {{4, 5,6},
      				{3,-1,7},
      				{2, 1,8}};
    
    //Définition d'entier utilisé comme des booléen dans les conditions du calcul (0 ou 1 uniquement)
    int calculating = 1; //1 si le processus doit refaire une itération
    int anc_calculating = 1; //1 si le processus devait refaire une itération à l'itération -1
    int north_calculating = 1; //1 si le processus au dessus doit refaire une itération
    int south_calculating = 1; //1 si le processus en dessous doit refaire une itération
    
    //Tant que le processus ou un de ses voisins doit calculer son flot
    while (calculating + north_calculating + south_calculating >= 1){
      //Si le processus doit calculer son flot
      if (calculating == 1) {
        calculating = 0;
        for (int i = 1; i < nb_lignes_local + 1; ++i) {
          for (int j = 0; j < nb_cols; ++j) {
            if (flot_local[i * nb_cols + j] == -1){
              int res = 1;
              bool no_skip = true;
              for (int vy = -1; vy <= 1; ++vy){
                for (int vx = -1; vx <= 1; ++vx){
                  if (0 <= i + vy && i + vy < nb_lignes_local + 2 && 0 <= j + vx && j + vx < nb_cols){
                    if (dir_local[(i+vy) * nb_cols + j + vx] == inv_direction[vy+1][vx+1]){
                      if (flot_local[(i+vy) * nb_cols + j + vx] != -1) {
                        res += flot_local[(i+vy) * nb_cols + j + vx];
                      }
                      else {
                        //Une valeur à empêché le calcul d'une valeur
                        no_skip = false;
                        //Le processus doit refaire une nouvelle itération
                        calculating = 1;
                      }
                    }
                  }
                }	
              }
              if (no_skip){
                flot_local[i*nb_cols + j] = res;
              }
            }
          }
        }
        //Envoie des ghosts aux processus n'ayant pas terminés leurs calculs
        if (north_calculating == 1){
          MPI_Isend(flot_local+nb_cols, nb_cols, MPI_INT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, &request);
        }
        if (south_calculating == 1){
          MPI_Isend(flot_local+nb_cols*nb_lignes_local, nb_cols, MPI_INT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, &request);
        }
      }

      //Réception des ghosts si le processus et son voisin devaient faire une nouvelle itérations
      if (anc_calculating == 1 && south_calculating == 1){
        MPI_Recv(flot_local+(nb_lignes_local+1)*nb_cols, nb_cols, MPI_INT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }
      if (anc_calculating == 1 && north_calculating == 1){
        MPI_Recv(flot_local, nb_cols, MPI_INT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }

      //Envoie des booléens concernant une nouvelle itération si on devait faire une itération
      if (anc_calculating == 1){
        MPI_Isend(&calculating, 1, MPI_INT, (mpi_rank+1)%mpi_size, 0, MPI_COMM_WORLD, &request);
        MPI_Isend(&calculating, 1, MPI_INT, (mpi_size+mpi_rank-1)%mpi_size, 1, MPI_COMM_WORLD, &request);
      }

      //Réception des booléens des processus voisins qui devaient refaire une itération
      if (north_calculating == 1)
        MPI_Recv(&north_calculating, 1, MPI_INT, (mpi_size+mpi_rank-1)%mpi_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      if (south_calculating == 1)
        MPI_Recv(&south_calculating, 1, MPI_INT, (mpi_rank+1)%mpi_size, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      //Mise à jour du booléens possédé par les voisins
      anc_calculating = calculating;
    /*if (mpi_rank == mpi_size -1){  
    printf("Mon pid %d\n",mpi_rank);
    for (int i = 0; i < nb_lignes_local + 2; ++i){
      for (int j = 0; j < nb_cols; ++j){
        printf("%d ", flot_local[i*nb_cols + j]);
      } 
      printf("\n");
    }
    }*/
        
  }

  //Création du tableau de flots globales pour le processus 0
  if (mpi_rank == 0)
    flot = (int *) calloc(nb_lignes*nb_cols,sizeof(int));
  //Rassemblement des flots locales dans le tableau de flots globales sur le processus 0
  MPI_Gather(flot_local + nb_cols, nb_lignes_local*nb_cols, MPI_INT, flot, nb_lignes_local*nb_cols, MPI_INT, 0, MPI_COMM_WORLD);  


    // 5. l'appel à do_tile
    if (mpi_rank == 0){
      //Affichage du tableau de flots globales
      /*printf("Mon pid %d\n",mpi_rank);
      for (int i = 0; i < nb_lignes; ++i){
        for (int j = 0; j < nb_cols; ++j){
          printf("%4.1d ", flot[i*nb_cols + j]);
        } 
        printf("\n");
      }*/
      mnt_do_tile_default(0,0,DIM,DIM);
    }
    else{
      do_tile(mpi_rank*(nb_lignes_local)*nb_cols, 0, nb_cols, nb_lignes_local, mpi_rank);
    }

    // 6. Le rassemblement pour avoir l'image compléte sur le processus root (=0).
    
  }
  return 0;
}
/////////////////////////////////////////////////////////////////

void lecture_mnt(char* nom)
{
  FILE* f = fopen(nom, "r");
  if (f != NULL) {
    int tmp;
    if (fscanf(f, "%d", &tmp)==1);
    nb_lignes = tmp;
    if (fscanf(f, "%d", &tmp)==1);
    nb_cols = tmp;
    if (fscanf(f, "%d", &tmp)==1);
    if (fscanf(f, "%d", &tmp)==1);
    if (fscanf(f, "%d", &tmp)==1);
    if (fscanf(f, "%f", &no_value)==1);


    terrain = (float*) calloc((2+nb_lignes)*nb_cols,sizeof(float));
    for (int j=0; j<nb_cols; j++){
      terrain[j] = no_value;
      terrain[(nb_lignes+1)*nb_cols+j] = no_value;
    }
    
    for (int i = 0; i < nb_lignes; i++)
      for (int j = 0; j < nb_cols; j++)
	if (fscanf(f, "%f", &(terrain[(i+1) * nb_cols + j]))==1);    
  }

  for (int j=0; j<nb_cols; j++) {
    terrain[j] = no_value;
    terrain[(nb_lignes+1)*nb_cols+j] = no_value;
  }
}

int f_bord1(float ref, float* tab, float no_value)
{
    float min = ref;
    int code =0;
    for (int i=0; i<5; i++)
        if (tab[i]!=no_value) {
            if (tab[i] < min) {
                min = tab[i];
                code = i + 1;
            }
        }
    return code;
}
int f_bord2(float ref, float* tab, float no_value)
{
    float min = ref;
    int code = 0;
    for (int i=0; i<5; i++){
        if (tab[i]!=no_value) {
            if (tab[i]<min) {
                min=tab[i];
                code=i;
            }
        }
    }
    if (code==4)
      return 1;
    else if(min != ref) //Prise en compte de la cuvette
      return code+5;
    return code;
}

int f(float ref, float* tab, float no_value)
{

    float min = ref;
    int code = 0;
    for (int i=0; i<8; i++)
        if (tab[i]!=no_value) {
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
      x = i+1 ;
      x1 = x - 1;
      x2 = x + 1;
      for (int j = 0; j < n_c; j++) {
	y = j;
	y1 = y-1;
	y2 = y+1;
	float val = data[x * n_c + y];
	
	if (val != no_value) {
	  if (j==0) {
	    tab_bord[0] = data[x1 * n_c + y];
	    tab_bord[1] = data[x1 * n_c + y2];
	    tab_bord[2] = data[x * n_c + y2];
	    tab_bord[3] = data[x2 * n_c + y2];
	    tab_bord[4] = data[x2 * n_c + y];
	    dir[i*n_c + j] = f_bord1(val,tab_bord,no_value);
	  }
	  else if (j==(n_c-1)) {
	    tab_bord[0] = data[x2 * n_c + y];
	    tab_bord[1] = data[x2 * n_c + y1];
	    tab_bord[2] = data[x * n_c + y1];
	    tab_bord[3] = data[x1 * n_c + y1];
	    tab_bord[4] = data[x1 * n_c + y];
	    dir[i*n_c + j] = f_bord2(val,tab_bord,no_value);
	  }
	  else {
	    tab[0] = data[x1 * n_c + y];
	    tab[1] = data[x1 * n_c + y2];
	    tab[2] = data[x * n_c + y2];
	    tab[3] = data[x2 * n_c + y2];
	    tab[4] = data[x2 * n_c + y];
	    tab[5] = data[x2 * n_c + y1];
	    tab[6] = data[x * n_c + y1];
	    tab[7] = data[x1 * n_c + y1];
	    dir[i* n_c + j]=f(val, tab,no_value);
	  }
	}
      }
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
