/**** numeris.c */

/********************** Numerisation Automatique des cotes ********************/

/* Version avec segmentation directionnelle et analyse locale */ 

/********************************** Includes **********************************/

/* Xlib version 11 */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/MwmUtil.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <math.h>
#include "header.h"
#include "igb_io.h"
#include "icon_nac"


/******************************************************************************/


/********************************** Declarations ******************************/

#define BORDER_W          3     
#define MAXCOLORS       256
#define NB_GRAY_NAME    101     /* Nb de niveaux de gris connus */

#define BUFF_SIZE       100     /* Nb de points max a entrer par l'usager */
#define SEUIL_GRAD       30     /* Seuil sur le module du gradient */
#define SEUIL_LAP         0     /* Seuil sur le Laplacien */

/******************************************************************************/

  
/********************************** Prototypes ********************************/

extern  short   **igb_read();
extern  char    *ten2eight_bits();
void segmente(short**, short**, short**, short**, short**, short**, 
	      short, short);
void seuillage(short**, short**, short**, short**, short**, 
	       short, short);
void locale(short**, short**,short**, short**, short**, short**, 
	       short, short, XPoint*);
void affiche(short**, short, short, short, char*, char**, int, XPoint*, char*);
void AllocateGrayNamedColor(void);
void AllocateNamedColorMap( char *cmap_file );
void SetImageWindow( short dimx, short dimy, char *image );


/******************************************************************************/


/******************************* Variables globales  **************************/

static char     *progname;
int             screen_num;
Display         *display;
Window          win, root, child;
unsigned int    keys_buttons;
GC              gc;
Pixel           ColorMap[MAXCOLORS];

/******************************************************************************/


void main( int argc, char *argv[] )
{

    int i, j;
    short **buffer;     /* image d'entree */
    short **binaire;    /* image binaire  */
    short **grad_mod, **grad_phase, **laplacien, **cadran, **cote;
    short  dimx, dimy, itype;
    Header head;
    char *nom_fich = "cote.igb";

    char *input_file, *window_name;
    int count;
    XPoint points[BUFF_SIZE]; /* points a entrer sur la cote */

    printf("\n\nN.A.C : Numerisation Automatique des Cotes\n");

    /* lecture ligne de commande */
    if(argc < 2) 
    {
	printf("\nUsage : num fichier<.igb|.Rx>\n\n");
	exit(-1);
    }

    input_file = (char *) malloc( strlen(argv[1]) );
    strcpy( input_file, argv[1] );


    if ( *input_file == NULL )
    {
	fprintf( stderr, "\n\n%c Error, nom de fichier d'entree manquant.\n\n", 0x07 );
	exit( 0 );
    }
    else
    {
	/* Verifier si il y a extension 'igb' */
	if ( strstr( input_file, ".igb" ) == NULL )
	    if ( strstr( input_file, ".Rx" ) == NULL )
		strcat( input_file, ".igb" );

	/* Rajoute le nom de fichier d'entree au nom de la fenetre */
	window_name = (char *) malloc( strlen( input_file ) + 10 );
	strcpy( window_name, "NAC : " );

	/* Remonter au premier path */
	count = strlen( input_file );
	while( input_file[count] != '/' && count != 0 )
	    count--;
	if ( count != 0 )
	    strcat( window_name, ".." );
	strcat( window_name, &input_file[count] );
    }


    /* Copier le nom du programme */
    progname = argv[0];

    /* lecture de l'image d'entree */
    buffer = igb_read(argv[1], &head);
    itype = head.type;
    dimx  = head.x;
    dimy  = head.y;


    /* allocation memoire */
    grad_mod = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	grad_mod[i] = (short *) malloc( dimx * sizeof( short ) );

    grad_phase = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	grad_phase[i] = (short *) malloc( dimx * sizeof( short ) );

    laplacien = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	laplacien[i] = (short *) malloc( dimx * sizeof( short ) );

    binaire =  (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	binaire[i] = (short *) malloc( dimx * sizeof( short ) );

    cadran =  (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	cadran[i] = (short *) malloc( dimx * sizeof( short ) );

    cote = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	cote[i] = (short *) malloc( dimx * sizeof( short ) );

   /*
for(i=1; i < 4; i++)
	printf("\n%d %d\n", points[i].x, points[i].y);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           */

    printf("\nSegmentation de l'image en cours...\n");

    /* segmentation de l'image */
    segmente( buffer, binaire, grad_mod, grad_phase, laplacien, cadran,
	      dimx, dimy );

    printf("\nEntrer trois points pour limiter la cote.\n");   
    printf("<ESC> pour finir.\n");


    /* affiche l'image segmente */
    affiche(binaire, itype, dimx, dimy, window_name,  argv, argc,
	    points, "cmp5");

    /* analyse locale */
    locale( binaire, cote, grad_mod, grad_phase, laplacien, cadran,
	    dimx, dimy, points );

    /* sauve l'image de la cote numerisee */
    head.type = SHORT;
    head.x    = dimx;
    head.y    = dimy;


    igb_write( cote, nom_fich, &head );
    igb_write( binaire, "_bin.igb", &head );

    /* affiche l'image de la cote numerisee */
    affiche(cote, itype, dimx, dimy, window_name,  argv, argc, 
	    points, "cmp5");

    /* Libere l'espace memoire */
    for ( i = 0; i < dimy; i++ )
    {
	free(grad_mod[i]);
	free(grad_phase[i]);
	free(laplacien[i]);
	free(binaire[i]);
	free(buffer[i]);
	free(cadran[i]);
	free(cote[i]);
    }

    free(grad_mod);
    free(grad_phase);
    free(laplacien);
    free(binaire);
    free(buffer);
    free(cadran);
    free(cote);

}


/************************************* segmente  ******************************/
/*

-Description   
     
	A partir de l'image originale on produit une image binaire et on calcule
	le gradient ainsi que le Laplacien

-Variable d'entree 

	buffer     : image originale

-Variables de sortie 

	binaire    : image binaire apres segmentation
	grad_mod
	grad_phase : gradient module et phase
	laplacien  : laplacien du point
	cadran     : indique dans quel cadran se situe la pente locale

				  ^
				1 | 4
				------>
				2 | 3

		signification physique >                          
		cadrans 2 et 4 : cotes dorsales poumon droit et ventrales gauche
		cadrans 1 et 3 : cotes ventrales poumon droit et dorsales gauche
				 (voir p. 60 chap.5 these de Wechsler)

	dimx, dimy : dimensions de l'image d'entree

*/
/******************************************************************************/

void segmente( short **buffer, short **binaire, short **grad_mod, 
	       short **grad_phase, short **laplacien, short **cadran,
	       short  dimx, short dimy )
{

    int i, j;
    int somme;
    register x, y;
    double grad_x, grad_y;
    short phase;
    short mask[3][3];
    short d;
   
 
    /* masque du Laplacien a gain normalise */

    mask[0][0] = (short) -1, mask[0][1] = (short) -1, mask[0][2] = (short) -1;
    mask[1][0] = (short) -1, mask[1][1] = (short)  8, mask[1][2] = (short) -1; 
    mask[2][0] = (short) -1, mask[2][1] = (short) -1, mask[2][2] = (short) -1; 
    d = 8; /* Normalisation */
	

    /* calcul du gradient et du Laplacien */

    for (y = 1; y < dimy- 1; y++)
    {
	for (x= 1; x < dimx- 1; x++)
	{
	    /* calcul de l'operateur de Sobel */
	    grad_x = (double) \
		     ( 2 * (buffer[y+1][x]   - buffer[y-1][x])   + \
			   (buffer[y+1][x+1] - buffer[y-1][x+1]) + \
			   (buffer[y+1][x-1] - buffer[y-1][x-1]) );

	    grad_y = (double) \
		     ( 2 * (buffer[y]  [x-1] - buffer[y]  [x+1]) + \
			   (buffer[y-1][x-1] - buffer[y-1][x+1]) + \
			   (buffer[y+1][x-1] - buffer[y+1][x+1]) );

	    /* calcul de la phase du gradient */
	    if ( grad_x != 0.0 )
		phase = (short) (atan (grad_y / grad_x) * 57.29578);
	    else
		phase = 90;

	    if ( grad_x > 0.0 )
		phase += 180;

	    if( phase >= 360 )
		phase -= 360;

	    grad_phase[y][x] = phase;
	    
	    /* calcul de la pente locale */
	    phase = phase - 90;

	    /* determine le cadran selon la pente de la courbe */
	    if(phase < 0)
		phase += 360;
	   
	    if( (phase >= 0) && (phase < 90) )
		cadran[y][x] = 4;
	    else
		if( (phase >= 90) && (phase < 180) )
		     cadran[y][x] = 1;
		else
		    if( (phase >= 180) && (phase < 270) )
			 cadran[y][x] = 2;
		    else
			if( (phase >= 270) && (phase < 360) )
			    cadran[y][x] = 3;

	    /* calcule le module */
	    grad_mod[y][x] = (short) sqrt( grad_x * grad_x + grad_y * grad_y );

	    /* calcul du Laplacien */
	    laplacien[y][x] = (short)( (buffer[y-1][x-1] * mask[0][0] + \
				  buffer[y-1][x]   * mask[0][1] + \
				  buffer[y-1][x+1] * mask[0][2] + \
				  buffer[y][x-1]   * mask[1][0] + \
				  buffer[y][x]     * mask[1][1] + \
				  buffer[y][x+1]   * mask[1][2] + \
				  buffer[y+1][x-1] * mask[2][0] + \
				  buffer[y+1][x]   * mask[2][1] + \
				  buffer[y+1][x+1] * mask[2][2]) / d );

	}/* for x */
    } /* for y */



    /* Retourne une image binaire apres seuillage */
    seuillage( binaire, grad_mod, grad_phase, laplacien, cadran,
	       dimx, dimy );

} /* segmente */
 


/*********************************** seuillage  *******************************/
/*

-Description   
     
	A partir du gradient, du Laplacien et de l'information sur l'orientation
	on produit une image binaire 

-Variables d'entree 

	grad_mod
	grad_phase : gradient module et phase
	laplacien  : laplacien du point
	cadran     : indique dans quel cadran se situe la pente locale 
	dimx, dimy : dimensions de l'image d'entree

-Variables de sortie 

	binaire    : image binaire apres segmentation

*/
/******************************************************************************/


void seuillage( short **binaire, short **grad_mod, 
		short **grad_phase, short **laplacien, short **cadran,
		short  dimx, short dimy )
{
    int i, j;
    register x, y;

    for (y =1; y < dimy-1; y++)
    {
	for (x=1; x < dimx-1; x++)
	{
		
	    /* seuillage */ 
	   
	    if( grad_mod[y][x] >= SEUIL_GRAD) 
	    {
		  grad_phase[y][x]= (short)( abs(grad_phase[y][x]) );
		  /* cotes dorsales poumon gauche et ventrales poumon droit */
		  if( ((cadran[y][x] ==1) || (cadran[y][x]==3)) )
		  {
		    if(laplacien[y][x] >= SEUIL_LAP)
		    {
			binaire[y][x] = (short) 500;      /* contour externe */ 
		    }
		    else
		    {
			if(laplacien[y][x] < SEUIL_LAP)
			    binaire[y][x] = (short) 1023;/*1023contour interne*/
		    }
		  }
		  /* cotes dorsales poumon droit et ventrales poumon gauche */
		  else
		  {
		    if(laplacien[y][x] >= SEUIL_LAP)
		    {
			 binaire[y][x] = (short) 800;/* 800 contour externe */  
		    }
		    else
		    {
			if(laplacien[y][x] < SEUIL_LAP)
			    binaire[y][x] = (short) 900; /* contour interne */
		    }
	
		  }

 
	    }
	    else
		binaire[y][x] = (short) 0;

	}/* for x */
    } /* for y */

}/* seuillage */


/************************************ locale  *********************************/
/*

-Description   
     
	A partir de l'image binaire et de l'information sur l'orientation 
	on analyse localement les contours des cotes pour les numeriser. 

-Variables d'entree 

	grad_mod
	grad_phase : gradient module et phase
	laplacien  : laplacien du point
	cadran     : indique dans quel cadran se situe la pente locale 
	dimx, dimy : dimensions de l'image d'entree
	pts        : les points entres par l'usager sur la cote
	binaire    : image binaire apres segmentation

-Variables de sortie 

	cote       : image de la cote numerisee  

*/
/******************************************************************************/


void locale( short **binaire, short** cote, short **grad_mod, 
	     short **grad_phase, short **laplacien, short **cadran,
	     short  dimx, short dimy, XPoint *pts )

{

    int i;
    int x, y;                           /* coordonnees du point examine */
    int R1, R2, L1, L2,
	D1, D2, U1, U2;                 /* booleens pour connectivite */ 
    int fin = 0;                        /* indique la fin de la recherche */

/* initialise l'image de la cote */
for (y =0; y < dimy; y++)
    for (x=0; x < dimx; x++)
	 cote[y][x] = (short) 0;


/* on part avec le point entre par l'usager */
/* patient #1053072 cote avec pt init. x=291 y=250 */
x = pts[1].x; y = pts[1].y;

printf("\nx=%d y=%d\n", x, y);

/* Analyse locale et suivi de contours */
while(!fin)
{

   /* determine la similarite de la direction a 10% pres */
   grad_phase[y][x]= (short)( abs(grad_phase[y][x]) );

   U1 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x-1]))
      &&  (grad_phase[y][x] >= (short)(0.90 * grad_phase[y-1][x-1])) 
      &&  (binaire[y-1][x-1] != 0) )? 1:0;

   U2 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y-1][x])) 
      &&  (binaire[y-1][x] != 0) )? 1:0;

   D1 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x])) 
      &&  (binaire[y+1][x] != 0) )? 1:0;

   D2 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x+1]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x+1]))
      &&  (binaire[y+1][x+1] != 0) )? 1:0;

   L1 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y][x-1]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y][x-1]))
      &&  (binaire[y][x-1] != 0) )? 1:0;

   L2 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x-1]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x-1]))
      &&  (binaire[y+1][x-1] != 0) )? 1:0;

   R1 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x+1]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y-1][x+1]))
      &&  (binaire[y-1][x+1] != 0) )? 1:0;

   R2 = ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y][x+1]))
      &&  (grad_phase[y][x] >= (short) (0.90 * grad_phase[y][x+1]))
      &&  (binaire[y][x+1] != 0) )? 1:0;

   printf("\n L1 = %d L2 = %d D1 = %d\n", L1, L2, D1);


   /* indique si on passe a la cote ventrale */
   fin = (y < pts[2].y) ? 0:1;

   /* examine la connectivite du point courant  */
   /* on passe au point suivant */
   /* cote dorsale poumon droit */
   if(L1)
   {
	x = x - 1;
	cote[y][x] = 1023;

   }
   else
   {
	if(L2)
	{
	     x = x - 1;
	     y = y + 1;
	     cote[y][x] = 1023;

	}
	else
	{
	     if(D1)
	     {
		   y = y + 1;
		   cote[y][x] = 1023;

	     }
	     else
	     {
		  fin = 1;
	     }
	}     
   }/* if */
   
   printf("\n fin = %d\n", fin);


}/* while */



}/* locale */


