/* Copyright 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  biq.c - simple greyscale quantization.
 *
 *	9/19/88
 */

#include "ciq.h"


biq(dith,nw,synth,cm)
int dith;		/* is dithering desired? 0=no, 1=yes */
int nw;			/* number of colors wanted in output image */
int synth;		/* synthesize colormap? 0=no, 1=yes */
colormap cm;		/* quantization colormap */
			/* read if synth=0; always written */
{
    colormap ocm;

    picreadcm(ocm);	/* read original picture's colormap (usually identity)*/

    for (n = 0; n < nw; n++)			/* also sets n to nw */
	color[0][n] = color[1][n] = color[2][n] = (n*256L+128)/nw;

    picwritecm(color);

    draw_grey(ocm);

    bcopy(color,cm,sizeof color);
}

/*----------------------------------------------------------------------*/

draw_grey(ocm)
colormap ocm;
{
    register rgbpixel *linin;
    register pixel *linout;
    int y;
    register int x;

    linin = line3alloc(xmax);
    linout = linealloc(xmax);

    for (y = 0; y < ymax; y++) {
	picreadline3(y, linin);
	for (x = 0; x < xmax; x++) {
		linin[x].r = ocm[0][linin[x].r];
		linin[x].g = ocm[1][linin[x].g];
		linin[x].b = ocm[2][linin[x].b];
		linout[x] = rgb_bright(&linin[x]);
		linout[x] = (linout[x]*n+n/2)/256;
	}
	picwriteline(y, linout);
    }
    free((char *)linin);
    free((char *)linout);
}
