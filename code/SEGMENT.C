/**** numeris.c */

/********************** Numerisation Automatique des cotes ********************/
/* 

-Version avec subdivision et calcul du seuil par moyenne du gradient

 
-Description 

        Le programme effectue une segmentation de la radiographie avec 
	un seuillage statistique. L'image est subdivisee en 10x10 sections,
	dont on determine le gradient moyen de maniere a definir un seuil
	local. Il affiche l'image segmentee et la conserve sous format igb.
	
 

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
#define BUFF_SIZE       100     /* Nb de points max a entrer sur la cote */
#define SECTION          15     /* Nb de subdivision dans l'image */

/******************************************************************************/

  
/********************************** Prototypes ********************************/

extern  short   **igb_read();
extern  char    *ten2eight_bits();
void segmente(short**, short**, short**, short**, short**, short**, 
	      short*, short, short);
void seuillage(short**, short**, short**, short**, short**, 
	      short*, short, short);
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
    short **grad_mod, **grad_phase, **laplacien, **cadran;
    short *seuil_grad;
    short  dimx, dimy, itype;
    Header head;
    char *nom_fich = "_bin.igb";

    char *input_file, *window_name;
    int count;
    XPoint points[BUFF_SIZE]; /* points a entrer sur la cote */

    printf("\n\nN.A.C : Numerisation automatique des cotes\n");
    printf("\nEntrer trois points sur la cote a numeriser.\n");
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

    seuil_grad =  (short *) malloc( SECTION * SECTION * sizeof( short ) );



    /* affiche l'image originale */
    affiche(buffer, itype, dimx, dimy, window_name,  argv, argc, points, NULL);

    /* segmentation de l'image */
    segmente( buffer, binaire, grad_mod, grad_phase, laplacien, cadran,
	      seuil_grad, dimx, dimy );


   
    /* sauve les contours */
    head.type = SHORT;
    head.x    = dimx;
    head.y    = dimy;


    igb_write( binaire, nom_fich, &head );

    /* affiche l'image segmente */
    affiche(binaire, itype, dimx, dimy, window_name,  argv, argc, 
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
    }

    free(grad_mod);
    free(grad_phase);
    free(laplacien);
    free(binaire);
    free(buffer);
    free(cadran);
    free(seuil_grad);
  
}/* main */

/******************************************************************************/

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
	       short *seuil_grad, short  dimx, short dimy )
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

	    if( phase > 360 )
		phase = 0;

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


    /* calcul des seuils locaux par la moyenne du gradient */

    for( j = 0; j < SECTION; j++)
    {
	for( i = 0; i < SECTION; i++)
	{
	somme = 0;

	for (y =(int)(dimy*j/SECTION); y < (int)(dimy*(j+1)/SECTION); y++)
	{
	    for (x=(int)(dimx*i/SECTION); x < (int)(dimx*(i+1)/SECTION); x++)
	    {
		
	 
		/* calcul la somme dans la section courante */
		somme += grad_mod[y][x];
    
	
	    }/* for x */
	} /* for y */
    
	/* calcule la moyenne de la section courante */
	seuil_grad[i+(SECTION*j)] = (short)( 1.2 *somme / (SECTION * SECTION) );

	} /* for i */
     } /* for j */


    /* Retourne une image binaire apres seuillage */
    seuillage( binaire, grad_mod, grad_phase, laplacien, cadran,
	      seuil_grad, dimx, dimy );

} /* segmente */

/*********************************** seuillage  *******************************/
/*

-Description   
     
	A partir du gradient, du Laplacien, du seuil de chaque
	section de la radiographie, et de l'image binaire on 
	produit une image segmentee. 

-Variables d'entree 

	grad_mod
	grad_phase : gradient module et phase
	seuil_grad : seuil de chacune des sections de l'image binaire
	laplacien  : laplacien du point
	cadran     : indique dans quel cadran se situe la pente locale 
	dimx, dimy : dimensions de l'image d'entree
	

-Variables de sortie 

	binaire    : image binaire apres segmentation et seuillage

*/
/******************************************************************************/


void seuillage( short **binaire, short **grad_mod, short **grad_phase, 
		short **laplacien, short **cadran, short *seuil_grad, 
		short  dimx, short dimy )
{
    short seuil_lap = 0;
    int i, j;
    register x, y;

/* Pour toutes les sections de l'image segmentee on compare le module
   du gradient au seuil local a la section. */
for( j = 0; j < SECTION; j++)
{
  for( i = 0; i < SECTION; i++)
  {

    for (y =(int)(dimy*j/SECTION); y < (int)(dimy*(j+1)/SECTION); y++)
    {
	for (x=(int)(dimx*i/SECTION); x < (int)(dimx*(i+1)/SECTION); x++)
	{
		
	    /* seuillage */ 
	   
	    if( (grad_mod[y][x] >= seuil_grad[i+(SECTION*j)]) )
	    {

		if(laplacien[y][x] >= seuil_lap)
		    binaire[y][x] = (short) 500;      /* contour externe */     
		else
		    if(laplacien[y][x] < seuil_lap)
			binaire[y][x] = (short) 1023; /* contour interne */
		
	    }
	    else
		binaire[y][x] = (short) 0;

	}/* for x */
    } /* for y */
  } /* for i */
} /* for j */


}/* seuillage */



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

