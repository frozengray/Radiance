/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert a trianglular mesh into a Radiance description.
 *
 * Unlike most other converters, we have defined a file
 * format for the input ourselves.  The format contains eight types,
 * each of which is identified by a single letter.  These are:
 *
 *	# comment		= a comment.  Continues until end of line.
 *	v id Px Py Pz		= a vertex.  The id must be an integer.
 *	n Nx Ny Nz		= a normal.  Corresponds to most recent vertex.
 *	i Iu Iv			= an index.  Corresponds to most recent vertex.
 *	p picture		= a picture.  Used as a pattern for following.
 *	m material		= a material name.  Used for what follows.
 *	o object		= an object name.  Used for what follows.
 *	t id1 id2 id3		= a triangle.
 *
 * Only the 't' type results in any output.  The others merely set values
 * to be used in generating triangles.  If no picture or "p -" is given,
 * there will be no pattern associated with the geometry.  If no material
 * or "m -" is given, no material will be associated.  (Note that this
 * only makes sense for a mesh which is to be put into an octree for
 * instancing.)  Using a pattern requires that each vertex have an
 * associated index value for generating the colorpict primitive.
 * Likewise, an interpolated surface normal also requires that each
 * vertex of the triangle have an associated normal vector.
 * It is not necessary for the normal vectors to have unit length.
 */

#include "standard.h"

#define VOIDID		"void"		/* this is defined in object.h */

#define CALNAME		"tmesh.cal"	/* the name of our auxiliary file */
#define PATNAME		"T-pat"		/* triangle pattern name (reused) */
#define TEXNAME		"T-nor"		/* triangle texture name (reused) */

#define V_DEFINED	01		/* this vertex is defined */
#define V_HASNORM	02		/* vertex has surface normal */
#define V_HASINDX	04		/* vertex has index */

typedef struct {
	short	flags;		/* vertex flags, from above */
	FVECT	pos;		/* location */
	FVECT	nor;		/* normal */
	float	ndx[2];		/* picture index */
} VERTEX;

VERTEX	*vlist = NULL;		/* our vertex list */
int	nverts = 0;		/* number of vertices in our list */

typedef FLOAT	BARYCCM[3][4];

#define novert(i)	((i)<0|(i)>=nverts || !(vlist[i].flags&V_DEFINED))

#define CHUNKSIZ	128	/* vertex allocation chunk size */

extern VERTEX	*vnew();	/* allocate a vertex (never freed) */

char	*defmat = VOIDID;	/* default (starting) material name */
char	*defpat = "";		/* default (starting) picture name */
char	*defobj = "T";		/* default (starting) object name */


main(argc, argv)		/* read in T-mesh files and convert */
int	argc;
char	*argv[];
{
	FILE	*fp;
	int	i;

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'o':		/* object name */
			defobj = argv[++i];
			break;
		case 'm':		/* default material */
			defmat = argv[++i];
			break;
		case 'p':		/* default picture */
			defpat = argv[++i];
			break;
		default:
			fprintf(stderr,
			"Usage: %s [-o obj][-m mat][-p pic] [file ..]\n",
					argv[0]);
			exit(1);
		}
	if (i >= argc)
		convert("<stdin>", stdin);
	else
		for ( ; i < argc; i++) {
			if ((fp = fopen(argv[i], "r")) == NULL) {
				perror(argv[i]);
				exit(1);
			}
			convert(argv[i], fp);
			fclose(fp);
		}
	exit(0);
}


convert(fname, fp)		/* convert a T-mesh */
char	*fname;
FILE	*fp;
{
	char	typ[4];
	int	id[3];
	double	vec[3];
	char	picfile[128];
	char	matname[64];
	char	objname[64];
	register int	i;
	register VERTEX	*lastv;
					/* start fresh */
	i = nverts;
	lastv = vlist;
	while (i--)
		(lastv++)->flags = 0;
	lastv = NULL;
	strcpy(picfile, defpat);
	strcpy(matname, defmat);
	strcpy(objname, defobj);

	printf("\n## T-mesh read from: %s\n", fname);
					/* scan until EOF */
	while (fscanf(fp, "%1s", typ) == 1)
		switch (typ[0]) {
		case 'v':		/* vertex */
			if (fscanf(fp, "%d %lf %lf %lf", &id[0],
					&vec[0], &vec[1], &vec[2]) != 4)
				syntax(fname, fp, "Bad vertex");
			lastv = vnew(id[0], vec[0], vec[1], vec[2]);
			break;
		case 't':		/* triangle */
			if (fscanf(fp, "%d %d %d", &id[0], &id[1], &id[2]) != 3)
				syntax(fname, fp, "Bad triangle");
			if (novert(id[0]) | novert(id[1]) | novert(id[2]))
				syntax(fname, fp, "Undefined triangle vertex");
			triangle(picfile, matname, objname, &vlist[id[0]],
					&vlist[id[1]], &vlist[id[2]]);
			break;
		case 'n':		/* surface normal */
			if (lastv == NULL)
				syntax(fname, fp, "No vertex for normal");
			if (fscanf(fp, "%lf %lf %lf",
					&vec[0], &vec[1], &vec[2]) != 3)
				syntax(fname, fp, "Bad vertex normal");
			lastv->nor[0] = vec[0];
			lastv->nor[1] = vec[1];
			lastv->nor[2] = vec[2];
			if (normalize(lastv->nor) == 0.0)
				syntax(fname, fp, "Zero vertex normal");
			lastv->flags |= V_HASNORM;
			break;
		case 'i':		/* index position */
			if (lastv == NULL)
				syntax(fname, fp, "No vertex for index");
			if (fscanf(fp, "%lf %lf", &vec[0], &vec[1]) != 2)
				syntax(fname, fp, "Bad index");
			lastv->ndx[0] = vec[0];
			lastv->ndx[1] = vec[1];
			lastv->flags |= V_HASINDX;
			break;
		case 'o':		/* object name */
			if (fscanf(fp, "%s", objname) != 1)
				syntax(fname, fp, "Bad object name");
			break;
		case 'm':		/* material */
			if (fscanf(fp, "%s", matname) != 1)
				syntax(fname, fp, "Bad material");
			if (matname[0] == '-' && !matname[1])
				strcpy(matname, VOIDID);
			break;
		case 'p':		/* picture */
			if (fscanf(fp, "%s", picfile) != 1)
				syntax(fname, fp, "Bad pattern");
			if (picfile[0] == '-' && !picfile[1])
				picfile[0] = '\0';
			break;
		case '#':		/* comment */
			fputs("\n#", stdout);
			while ((i = getc(fp)) != EOF) {
				putchar(i);
				if (i == '\n')
					break;
			}
			break;
		default:
			syntax(fname, fp, "Unknown type");
			break;
		}
}


triangle(pn, mod, obj, v1, v2, v3)	/* put out a triangle */
char	*pn, *mod, *obj;
register VERTEX	*v1, *v2, *v3;
{
	static int	ntri = 0;
	BARYCCM	bvecs;
					/* compute barycentric coordinates */
	if (v1->flags & v2->flags & v3->flags & (V_HASINDX|V_HASNORM))
		if (comp_baryc(bvecs, v1->pos, v2->pos, v3->pos) < 0)
			return;
					/* put out texture (if any) */
	if (v1->flags & v2->flags & v3->flags & V_HASNORM) {
		printf("\n%s texfunc %s\n", mod, TEXNAME);
		mod = TEXNAME;
		printf("4 dx dy dz %s\n", CALNAME);
		printf("0\n21\n");
		put_baryc(bvecs);
		printf("\t%14.12g %14.12g %14.12g\n",
				v1->nor[0], v2->nor[0], v3->nor[0]);
		printf("\t%14.12g %14.12g %14.12g\n",
				v1->nor[1], v2->nor[1], v3->nor[1]);
		printf("\t%14.12g %14.12g %14.12g\n",
				v1->nor[2], v2->nor[2], v3->nor[2]);
	}
					/* put out pattern (if any) */
	if (*pn && (v1->flags & v2->flags & v3->flags & V_HASINDX)) {
		printf("\n%s colorpict %s\n", mod, PATNAME);
		mod = PATNAME;
		printf("7 noneg noneg noneg %s %s u v\n", pn, CALNAME);
		printf("0\n18\n");
		put_baryc(bvecs);
		printf("\t%f %f %f\n", v1->ndx[0], v2->ndx[0], v3->ndx[0]);
		printf("\t%f %f %f\n", v1->ndx[1], v2->ndx[1], v3->ndx[1]);
	}
					/* put out triangle */
	printf("\n%s polygon %s.%d\n", mod, obj, ++ntri);
	printf("0\n0\n9\n");
	printf("%18.12g %18.12g %18.12g\n", v1->pos[0],v1->pos[1],v1->pos[2]);
	printf("%18.12g %18.12g %18.12g\n", v2->pos[0],v2->pos[1],v2->pos[2]);
	printf("%18.12g %18.12g %18.12g\n", v3->pos[0],v3->pos[1],v3->pos[2]);
}


int
comp_baryc(bcm, v1, v2, v3)		/* compute barycentric vectors */
register BARYCCM	bcm;
FLOAT	*v1, *v2, *v3;
{
	FLOAT	*vt;
	FVECT	va, vab, vcb;
	double	d;
	register int	i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			vab[i] = v1[i] - v2[i];
			vcb[i] = v3[i] - v2[i];
		}
		d = DOT(vcb,vcb);
		if (d <= FTINY)
			return(-1);
		d = DOT(vcb,vab)/d;
		for (i = 0; i < 3; i++)
			va[i] = vab[i] - vcb[i]*d;
		d = DOT(va,va);
		if (d <= FTINY)
			return(-1);
		for (i = 0; i < 3; i++) {
			va[i] /= d;
			bcm[j][i] = va[i];
		}
		bcm[j][3] = -DOT(v2,va);
					/* rotate vertices */
		vt = v1;
		v1 = v2;
		v2 = v3;
		v3 = vt;
	}
	return(0);
}


put_baryc(bcm)				/* put barycentric coord. vectors */
register BARYCCM	bcm;
{
	register int	i;

	for (i = 0; i < 3; i++)
		printf("%14.8f %14.8f %14.8f %14.8f\n",
				bcm[i][0], bcm[i][1], bcm[i][2], bcm[i][3]);
}


VERTEX *
vnew(id, x, y, z)			/* create a new vertex */
register int	id;
double	x, y, z;
{
	register int	i;

	if (id > nverts) {		/* get some more */
		i = nverts;
		nverts = CHUNKSIZ*((id%CHUNKSIZ)+1);
		if (vlist == NULL)
			vlist = (VERTEX *)malloc(nverts*sizeof(VERTEX));
		else
			vlist = (VERTEX *)realloc((char *)vlist,
					nverts*sizeof(VERTEX));
		if (vlist == NULL) {
			fprintf(stderr,
			"Out of memory while allocating vertex %d\n", id);
			exit(1);
		}
		while (i < nverts)		/* clear what's new */
			vlist[i++].flags = 0;
	}
					/* assign new vertex */
	vlist[id].pos[0] = x;
	vlist[id].pos[1] = y;
	vlist[id].pos[2] = z;
	vlist[id].flags = V_DEFINED;
					/* return it */
	return(&vlist[id]);
}


syntax(fn, fp, er)			/* report syntax error and exit */
char	*fn;
register FILE	*fp;
char	*er;
{
	extern long	ftell();
	register long	cpos;
	register int	c;
	int	lineno;

	if (fp == stdin)
		fprintf(stderr, "%s: T-mesh format error: %s\n", fn, er);
	else {
		cpos = ftell(fp);
		fseek(fp, 0L, 0);
		lineno = 1;
		while (cpos-- > 0) {
			if ((c = getc(fp)) == EOF)
				break;
			if (c == '\n')
				lineno++;
		}
		fprintf(stderr, "%s: T-mesh format error at line %d: %s\n",
				fn, lineno, er);
	}
	exit(1);
}
