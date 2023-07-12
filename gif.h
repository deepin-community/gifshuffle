/*
 * GIF routines.
 *
 * Written by Matthew Kwan - January 1998
 */

#ifndef _GIF_H
#define _GIF_H

#include <stdio.h>

typedef struct {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} RGB;

typedef struct {
	int		gi_bits_per_pixel;
	int		gi_num_colours;
	RGB		gi_colours[256];
	RGB		gi_orig_colours[256];
	unsigned char	gi_header[13];
} GIFINFO;


/*
 * Define external functions.
 */

extern BOOL	gif_header_load (GIFINFO *gi, FILE *fp);
extern BOOL	gif_filter_save (const GIFINFO *gi, FILE *infp, FILE *outfp);

#endif
