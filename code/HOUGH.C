/**** numeris.c */

/********************** Numerisation Automatique des cotes ********************/

/* 

-Version avec segmentation directionnelle et transformee de Hough.
 
-Description 

	Dans le cadre de la these de Wechsler, le programme effectue une 
	segmentation directionnelle et calcule la transformee de Hough 
	d'une radiographie. Il affiche la position des foyers de parabole 
	des cotes et genere l'image segmentee et l'image des foyers en 
	format igb.


-Plateforme 

	Le programme a ete concu pour l'environnement X Windows.

-Auteur : Marc Missonnier, matricule 45302, Ecole Polytechnique de Montreal

-Date   : le 10 septembre 1993 


*/
/******************************************************************************/


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
#define MAX_SEG         500     /* Nb max de segments sur la cote */
#define MAX_PTS        1000     /* Nb max de points par segment */
/******************************************************************************/

  
/********************************** Prototypes ********************************/

extern  short   **igb_read();
extern  char    *ten2eight_bits();
void segmente(short**, short**, short**, short**, short**, short**, 
	      short, short);
void seuillage(short**, short**, short**, short**, short**, 
	       short, short);
void hough(short**, short**,short**, short**, short**, short**, 
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
    short **grad_mod, **grad_phase, **laplacien, **cadran, **acc;
    short  dimx, dimy, itype;
    Header head;
    char *nom_fich = "foyer.igb";

    char *input_file, *window_name;
    int count;
    XPoint points[BUFF_SIZE]; /* points a entrer sur la cote */

    printf("\n\nN.A.C : Numerisation Automatique des Cotes\n");
    printf("\nEntrer deux points pour limiter le thorax.\n");
    printf("\nEntrer deux points pour limiter la zone de traitement.\n");    
    printf("<ESC> pour finir.\n");

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
	/* Verifier si il y a une extension 'igb' */
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

    acc = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	acc[i] = (short *) malloc( dimx * sizeof( short ) );

    /* affiche l'image */
    affiche(buffer, itype, dimx, dimy, window_name,  argv, argc, points, NULL);

    /* segmentation de l'image */
    segmente( buffer, binaire, grad_mod, grad_phase, laplacien, cadran,
	      dimx, dimy );


    /* Transformee de Hough */
    hough( binaire, acc, grad_mod, grad_phase, laplacien, cadran,
	   dimx, dimy, points );

    /* sauve les foyers des paraboles */
    head.type = SHORT;
    head.x    = dimx;
    head.y    = dimy;


    igb_write( acc, nom_fich, &head );
    igb_write( binaire, "_bin.igb", &head );

    /* affiche les foyers possibles */
    affiche(acc, itype, dimx, dimy, window_name,  argv, argc, 
	    points, "cmp1");

    /* Libere l'espace memoire */
    for ( i = 0; i < dimy; i++ )
    {
	free(grad_mod[i]);
	free(grad_phase[i]);
	free(laplacien[i]);
	free(binaire[i]);
	free(buffer[i]);
	free(cadran[i]);
	free(acc[i]);
    }

    free(grad_mod);
    free(grad_phase);
    free(laplacien);
    free(binaire);
    free(buffer);
    free(cadran);
    free(acc);

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

		/* Examine si la direction du gradient est semblable a 10% */

		grad_phase[y][x]= (short)( abs(grad_phase[y][x]) );
		if( ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x])) )
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x+1]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x+1])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y+1][x-1]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y+1][x-1])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y][x-1]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y][x-1])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y][x+1]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y][x+1])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y-1][x])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x+1]))
		 && (grad_phase[y][x] >= (short) (0.90 * grad_phase[y-1][x+1])))
		|| ( (grad_phase[y][x] <= (short) (1.10 * grad_phase[y-1][x-1]))
		 && (grad_phase[y][x] >= (short)(0.90* grad_phase[y-1][x-1]))) )
		{
		
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
			    binaire[y][x] = (short) 1023; /*1023 contour interne */
		    }
		  }
		  /* cotes dorsales poumon droit et ventrales poumon gauche */
		  else
		  {
		    if(laplacien[y][x] >= SEUIL_LAP)
		    {
			 binaire[y][x] = (short) 800;/* 800*/ /* contour externe */     
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
 
	    }
	    else
		binaire[y][x] = (short) 0;

	}/* for x */
    } /* for y */

}/* seuillage */


/************************************ hough  **********************************/
/*

-Description   
     
	A partir de l'image binaire et de l'information sur l'orientation 
	on effectue la transformee de Hough. 

-Variables d'entree 

	grad_mod
	grad_phase : gradient module et phase
	laplacien  : laplacien du point
	cadran     : indique dans quel cadran se situe la pente locale 
	dimx, dimy : dimensions de l'image d'entree
	points     : les points entres par l'usager sur la cote
	binaire    : image binaire apres segmentation
	pts        : points limites du thorax

-Variables de sortie 

	acc        : accumulation des foyers de paraboles  

*/
/******************************************************************************/


void hough( short **binaire, short** acc, short **grad_mod, 
	    short **grad_phase, short **laplacien, short **cadran,
	    short  dimx, short dimy, XPoint *pts )

{

    int i;
    int xp=0, yp=0;                     /* coordonnees du point examine */
    int xf, yf;                         /* coordonnnees du point focus */
    int R1, R2, L1, L2;                 /* booleens pour connectivite */
    short** ACC;                        /* accumulateur apres convolution */
    short mask[3][3];                   /* masque de convolution */
    int direc_gau, direc_droit;         /* directrice gauche et droite */

    /* masque pour l'accumulateur */

    mask[0][0] = (short) 1, mask[0][1] = (short) 2, mask[0][2] = (short) 1;
    mask[1][0] = (short) 2, mask[1][1] = (short) 4, mask[1][2] = (short) 2; 
    mask[2][0] = (short) 1, mask[2][1] = (short) 2, mask[2][2] = (short) 1; 

    /* allocation memoire de l'accumulateur */
    ACC = (short **) malloc( dimy * sizeof( short * ) );
    for ( i = 0; i < dimy; i++ )
	ACC[i] = (short *) malloc( dimx * sizeof( short ) );

   for (yp =0; yp < dimy; yp++)
	for (xp=0; xp < dimx; xp++)
	    acc[yp][xp] = 0;



    /* technique de Hough -> voir Wechsler chap. 5 */    
    /* detection des sections paraboliques avec l'accumulateur */

    direc_gau   = pts[1].x - 15; /* la valeur 15 est pure speculation */
    direc_droit = pts[2].x + 15;


    /* cotes DORSALES POUMON DROIT */
    for (yp=1; yp < dimy-1; yp++)
    {
	for (xp=pts[1].x; xp < pts[3].x; xp++)
	{


	    if( (binaire[yp][xp] != 0)
	      && ((cadran[yp][xp] == 4) || (cadran[yp][xp] == 4)) ) 
	    {
		
		/* calcule le foyer avec la pente locale */
		/* une directrice "flexible"  pourra se former au contour de 
		cage thoracique +/- une distance definit selon le niveau        
		vertebrale du pixel.  Cette distance pourra dependre des        
		parametres paraboliques, des cotes situees a ce niveau.  Et ces 
		parametres pourraient etre obtenus eventuellement d'un modele   
		parametrique de la cage thoracique. */  
		
		xf=xp -abs((xp-direc_gau) *sin(2 *(grad_phase[yp][xp]-90) /57.29578));
		yf=yp +abs((xp-direc_gau) *cos(2 *(grad_phase[yp][xp]-90) /57.29578));

		/* cas limites */
		if( xf < 0 )
		   xf = 0;
		if( yf < 0 )
		   yf = 0;
		if( yf >= dimy)
		    yf = dimy - 1;

		/* affecte un poids selon la similarite du cadran */
		/* examine si les voisins appartiennent a une arete */
		L1 = (cadran[yp][xp]==cadran[yp][xp-1])
		   && (binaire[yp][xp-1] != 0);
		L2 = (cadran[yp][xp]==cadran[yp+1][xp-1])
		   && (binaire[yp+1][xp-1] != 0);
		R1 = (cadran[yp][xp]==cadran[yp][xp+1])
		   && (binaire[yp][xp+1] != 0);
		R2 = (cadran[yp][xp]==cadran[yp-1][xp+1])
		   && (binaire[yp-1][xp+1] != 0);

		if( (L1 || L2) && (R1 || R2) )
		    acc[yf][xf] += 48;
		else
		    if( (L1 || L2) || (R1 || R2) )
			acc[yf][xf] += 24;
		    else
			acc[yf][xf] += 12;



	    }
	}
    }

    /* cotes DORSALES POUMON GAUCHE */
    for (yp=1; yp < dimy-1; yp++)
    {
	for (xp=pts[4].x; xp < pts[2].x; xp++)
	{
	    if( (binaire[yp][xp] != 0) 
	      && ((cadran[yp][xp] == 3) || (cadran[yp][xp] == 3)) ) 
	    {
		
		/* calcule le foyer avec la pente locale */
		xf=xp +abs((direc_droit-xp) *sin(2 *(grad_phase[yp][xp]-90) /57.29578));
		yf=yp +abs((direc_droit-xp) *cos(2 *(grad_phase[yp][xp]-90) /57.29578));

		/* cas limites */
		if( xf >= dimx  )
		   xf = dimx - 1;
		if( yf < 0 )
		   yf = 0;
		if( yf >= dimy)
		    yf = dimy - 1;

		/* affecte un poids selon la similarite du cadran */
		/* examine si les voisins appartiennent a une arete */
		L1 = (cadran[yp][xp]==cadran[yp][xp-1])
		   && (binaire[yp][xp-1] != 0);
		L2 = (cadran[yp][xp]==cadran[yp+1][xp-1])
		   && (binaire[yp+1][xp-1] != 0);
		R1 = (cadran[yp][xp]==cadran[yp][xp+1])
		   && (binaire[yp][xp+1] != 0);
		R2 = (cadran[yp][xp]==cadran[yp-1][xp+1])
		   && (binaire[yp-1][xp+1] != 0);

		if( (L1 || L2) && (R1 || R2) )
		    acc[yf][xf] += 48;
		else
		    if( (L1 || L2) || (R1 || R2) )
			acc[yf][xf] += 24;
		    else
			acc[yf][xf] += 12;



	    }
	}
    }

   
    /* convolution du tableau accumulateur */
    /*for (yp=1; yp < dimy-1; yp++)
    {
	for (xp=1; xp < dimx-1; xp++)
	{

	  ACC[yp][xp] = (short)( (acc[yp-1][xp-1] * mask[0][0] + \
				  acc[yp-1][xp]   * mask[0][1] + \
				  acc[yp-1][xp+1] * mask[0][2] + \
				  acc[yp][xp-1]   * mask[1][0] + \
				  acc[yp][xp]     * mask[1][1] + \
				  acc[yp][xp+1]   * mask[1][2] + \
				  acc[yp+1][xp-1] * mask[2][0] + \
				  acc[yp+1][xp]   * mask[2][1] + \
				  acc[yp+1][xp+1] * mask[2][2]) );
	}
    }
  
    acc = ACC;*/


    /* Methode de clustering pour determiner les foyers (C_mean / Isodata) */
    /* a completer ... */

    /* marque les points numerises dans l'image binaire */
    /* a completer ... */

    /* libere l'espace memoire */
    for ( i = 0; i < dimy; i++ )
	free(ACC[i]);
    
    free(ACC);


 

}/* hough */

/********************************* affiche  ***********************************/
/*

-Description   
     
	A partir de la table de couleur, on affiche dans une fenetre 
	l'image binaire. Et on retourne les points entres par l'usager.

-Variables d'entree 

	window_name : nom de la fenetre=  NAC: + nom de fichier
	itype       : type des donnees (short / char)
	dimx, dimy  : dimensions de l'image d'entree
	data        : image binaire a afficher
	table       : nom du fichier contenant la table des couleurs


-Variables de sortie 

	points      : les points entres par l'usager sur la cote

*/
/******************************************************************************/

void affiche(short **data, short itype, short dimx, short dimy,
	     char *window_name, char **argv, int argc, XPoint *points, 
	     char* table)
{

    char                *image;
    char *display_name= NULL;
    int                 count, i;
    int                 index= 1;
    char                buffer[10];
    int                 bufsize = 10, nchars;
    int                 pos_x, pos_y, root_x, root_y;
    Pixmap              icon_pixmap;
    XEvent              report;
    XGCValues           values;
    XSizeHints          size_hints;
    XIconSize           *size_list;
    KeySym              ks;
    XComposeStatus      cs;
    XWMHints            wm_hints;
    XClassHint          class_hints;
    XTextProperty       windowName, iconName;


/* voir XLib Prog. Manual vol. 1 sect. 9.2.1.2 pp. 288-289 */

    if ( (display = XOpenDisplay( display_name )) == NULL )
    {
	(void) fprintf( stderr, "%s: cannot connect to X server %s\n",
			progname, XDisplayName(display_name) );
	exit( -1 );
    }

    /* numero d'ecran par defaut */
    screen_num = DefaultScreen( display );

    if( table == NULL )
	/* echelle de gris */
	AllocateGrayNamedColor();
    else
	AllocateNamedColorMap(table);
	

    /* Conversion de 10 bits a 8 bits pour l'affichage */
    image = ten2eight_bits( data, itype, dimx, dimy );

    /* Creer une fenetre pour afficher l'image */
    win = XCreateSimpleWindow( display, RootWindow( display, screen_num ), 0, 0,
	  dimx, dimy, BORDER_W, BlackPixel( display, screen_num ), 
	  WhitePixel(display, screen_num ) );

    /* Trouve la taille des icones de window manager */
    if ( XGetIconSizes( display, RootWindow( display, screen_num ),
		 &size_list, &count ) == 0 )
	(void) fprintf( stderr, "%s: Window manager didn't set icon sizes - \
		using default.\n", progname );

    /* Creer un bitmap de l'icone */
    icon_pixmap = XCreateBitmapFromData( display, win, icon_nac_bits, 
		  icon_nac_width, icon_nac_height );

    /* definit les parametres */
    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.min_width = dimx;
    size_hints.min_height = dimy;

    
    /* conserve le nom de la fenetre et le nom de l'icone dans 
       la structure XTextProperty et definit ses autres champs */
    if ( XStringListToTextProperty( &window_name, 1, &windowName ) == 0 )
    {
	(void) fprintf( stderr, "%s: structure allocation for \
				windowName failed.\n", progname );
	 exit( -1 );
    }

    if ( XStringListToTextProperty( &progname, 1, &iconName ) == 0 )
    {
	(void) fprintf( stderr, "%s: structure allocation for \
				iconName failed.\n", progname );
	exit( -1 );
    }

    wm_hints.initial_state = NormalState;
    wm_hints.input = True;
    wm_hints.icon_pixmap = icon_pixmap;
    wm_hints.flags = StateHint | IconPixmapHint | InputHint;

    class_hints.res_name = progname;
    class_hints.res_class = "NAC";

    XSetWMProperties( display, win, &windowName, &iconName, argv, argc,
		&size_hints, &wm_hints, &class_hints );
    

    /* choisit les types d'evenement desires */
    XSelectInput( display, win, ExposureMask | KeyPressMask |
	    ButtonPressMask | ButtonReleaseMask |
	    PointerMotionHintMask);

    /* creer un contexte graphique par defaut */
    values.foreground = BlackPixel( display, screen_num );
    values.background = WhitePixel( display, screen_num );
    gc = XCreateGC( display, win, (GCForeground | GCBackground), &values );

    XSetFunction( display, gc, GXxor );

    /* affiche la fenetre */
    XMapWindow( display, win );

    /* examine les evenements */
    while( 1 )
    {
	XNextEvent( display, &report );
	switch ( report.type )
	{

	    case ButtonPress :          
		points[index].x = report.xbutton.x;
		points[index].y = report.xbutton.y;             
		break;

	    case ButtonRelease :
		/* affiche le point */
		XSetFunction( display, gc, GXinvert);
		XDrawLine( display, win, gc, points[index].x-10, 
			  points[index].y, points[index].x+10, points[index].y);
		XDrawLine( display, win, gc, points[index].x, 
		      points[index].y-10, points[index].x, points[index].y+10);
		index++;
		break;

	    case Expose :
	
		/* Affichage et rafraichissement de la fenetre */
		if ( report.xexpose.count == 0 )
		{       
		    XSetFunction( display, gc, GXcopy);   
		    SetImageWindow( dimx, dimy, image );
		    XSetFunction( display, gc, GXinvert);
		    for(i=1; i < index; i++)
		    {
			XDrawLine( display, win, gc, points[i].x-10, 
				   points[i].y, points[i].x+10, points[i].y);
			XDrawLine( display, win, gc, points[i].x, 
				   points[i].y-10, points[i].x, points[i].y+10);
		    }

		    break;
		}                                                               
		break;

	    case KeyPress :
		nchars = XLookupString( &report, buffer, bufsize, &ks, &cs );

		/* appuie sur ESC pour terminer */
		if ( buffer[0] == 0x1b )
		{
		    XFreeGC( display, gc );
		    XCloseDisplay( display );
		    return; 
		}
	    default :
		    /* Tous les evenements nom desires sont ignores */
		break;

	} /* End switch */

    } /* End while */


}/* affiche */



/************************* AllocateGrayNamedColor()  **************************/
/*

-Description   
	Cree un nouveau Colormap a l'aide des 100 nom standard de Gris       
	(Standard X : gray0 a gray99 et grey0 a grey99)                      
	       

*/
/******************************************************************************/
void AllocateGrayNamedColor( void )
{
    int      i;
    unsigned long j;
    char     grayname[7];
    XColor   colorcell_def, rgb_db_def;
    Colormap def_cmap;

    struct {
	int     index;
	Pixel   pixel;
    } look_up_gray[NB_GRAY_NAME]; 


    /* Allocation des niveaux de gris dans la table par defaut. */
    def_cmap = DefaultColormap( display, screen_num );
    for( i = 0; i < NB_GRAY_NAME; i++ )
    {
	sprintf( grayname, "gray%d", i );

	if( !XAllocNamedColor( display, def_cmap, grayname, &colorcell_def, &rgb_db_def ) )
	{
	    fprintf( stderr, "\n   color %s not available in rgb.txt", grayname );
	    look_up_gray[i].index = -1;
	    exit( 0 );
	}
	else
	{
	    look_up_gray[i].index = rgb_db_def.red>>8;
	    look_up_gray[i].pixel = colorcell_def.pixel;
	}
    }

    /* Creer une nouvelle table */
    for ( i = 1; i < NB_GRAY_NAME; i++ )
	for ( j = look_up_gray[i-1].index; j < look_up_gray[i].index; j++ )
	    ColorMap[j] = look_up_gray[i-1].pixel;

    ColorMap[look_up_gray[NB_GRAY_NAME-1].index] = look_up_gray[NB_GRAY_NAME-1].pixel;

}/* AllocateGrayNamedColor */


/*************************** SetImageWindow()  ********************************/
/*

-Description   
	Convertit les donnees de l'image en utilisant le ColorMap,           
	alloue une structure XImage et affiche l'image.                      
	       
-Variables d'entree 

	dimx, dimy  : dimensions de l'image d'entree
	image       : image binaire 8 bits a afficher

*/
/******************************************************************************/
void SetImageWindow( short dimx, short dimy, char *image )
{
    char                *new_image;
    XImage              *ximage;
    unsigned long       i;


    /* Allocation du buffer pour l`image. */
    new_image = (char *) malloc( dimx * dimy );

    /* Conversion des niveaux de gris */
    for ( i = 0; i < dimx * dimy - dimx; i++ )
	new_image[i] = (char) ColorMap[image[i]];

    /* Creer l'image. */
    ximage = XCreateImage( display, XDefaultVisual( display, screen_num ),
	    8, ZPixmap, 0, new_image, dimx, dimy, 8, dimx );

    /* Affiche Image */
    XPutImage( display, win, gc, ximage, 0, 0, 0, 0, dimx, dimy );

    /* Libere l'image */
    XDestroyImage( ximage );

}/* SetImageWindow */

	    
/*************************** AllocateNamedColorMap()  *************************/
/*

-Description   
	Lit le fichier Colormap de l'utilisateur, et fait l'allocation d'un  
	nouveau colormap.                                                    

-Variables d'entree 

	cmap_file     : fichier contenant la table de couleurs

*/
/******************************************************************************/
void AllocateNamedColorMap( char *cmap_file )
{
    FILE        *fin;
    char        colorname[25], s[80];
    Colormap    def_cmap;
    int         i, n, nitems, end = FALSE;
    XColor      colorcell_def, rgb_db_def;
    unsigned long j;

    struct {
	int     index;
	Pixel   pixel;
    } look_up_color[MAXCOLORS]; 


    /* Ouvrir le fichier de color map */
    if ( (fin = fopen( cmap_file, "r" )) == NULL )
    {
	fprintf( stderr, "\n Error, incapable d'ouvrir le fichier du colormap.\n\n" );
	exit( 0 );
    }

    /* Allocation des couleurs dans la table par defaut. */
    def_cmap = DefaultColormap( display, screen_num );
    nitems = 0;
    while ( !end )
    {
	/* Test l'index de look-up table */
	if ( nitems > MAXCOLORS )
	{
	    fprintf( stderr, "\n Error, colormap - look-up table overflow.\n\n" );
	    exit( 0 );
	}

	/* lit les noms de couleurs */
	if ( fscanf( fin, "%d %s", &n, colorname ) == EOF )
	    end = TRUE;
	else
	{
	    /* Alloue les noms de couleurs */
	    if( !XAllocNamedColor( display, def_cmap, colorname, &colorcell_def, &rgb_db_def ))
	    {
		fprintf(stderr, "\n   color %s not available in rgb.txt", colorname);
		look_up_color[nitems].index = -1;
		exit(0);
	    }
	    else
	    {
		/* construit look-up table */
		look_up_color[nitems].index = n;
		look_up_color[nitems].pixel = colorcell_def.pixel;
		nitems++;
	    }
	}
    }

    /* Affecte la table de couleurs */
    nitems--;
    for ( i = 0; i < nitems; i++ )
	for ( j = look_up_color[i].index; j < look_up_color[i+1].index; j++ )
	    ColorMap[j] = look_up_color[i].pixel;

    /* Completer la table jusqu'a 255 */
    i = look_up_color[nitems].index;
    do {
	ColorMap[i++] = look_up_color[nitems].pixel;
    } while( i < MAXCOLORS );

} /* AllocateNamedColorMap */
