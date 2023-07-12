/*
 * GIF routines.
 *
 * Written by Matthew Kwan - January 1998
 */

#include "gifshuf.h"
#include "gif.h"

#include <stdlib.h>


/*
 * Local variables.
 */

static int	block_size = 0;
static int	clear_code_index = 0;
static int	max_code = 0;
static BOOL	use_end_code = FALSE;


/*
 * Load the header of a GIF image.
 * Returns FALSE if the file is not a GIF.
 */

BOOL
gif_header_load (
	GIFINFO		*gi,
	FILE		*fp
) {
	unsigned char	*bp = gi->gi_header;
	unsigned char	buf[768];
	int		i, n;

	if (fread (bp, sizeof (char), 13, fp) != 13) {
	    fprintf (stderr, "Error: could not read header information.\n");
	    return (FALSE);
	}

	if (bp[0] != 'G' && bp[1] != 'I' && bp[2] != 'F') {
	    fprintf (stderr, "Error: not a GIF file.\n");
	    return (FALSE);
	}

	if ((bp[10] & 0x80) == 0) {
	    fprintf (stderr,
		"Error: GIF file does not have a global colourmap.\n");
	    return (FALSE);
	}

	gi->gi_bits_per_pixel = (bp[10] & 7) + 1;
	n = gi->gi_num_colours = 1 << gi->gi_bits_per_pixel;

	if (fread (buf, sizeof (char), n * 3, fp) != n * 3) {
	    fprintf (stderr, "Error: could not read colourmap.\n");
	    return (FALSE);
	}

	for (i=0; i<gi->gi_num_colours; i++) {
	    gi->gi_colours[i].r = gi->gi_orig_colours[i].r = buf[i*3];
	    gi->gi_colours[i].g = gi->gi_orig_colours[i].g = buf[i*3 + 1];
	    gi->gi_colours[i].b = gi->gi_orig_colours[i].b = buf[i*3 + 2];
	}

	return (TRUE);
}


/*
 * The GIF decoding code is based on giftoppm.c
 * As a result, I must include the following message.
 */
/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, David Koblas.				       | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.	       | */
/* +-------------------------------------------------------------------+ */


/*
 * Filter a GIF extension.
 * This involves passing it straight through, except for GIF89
 * extensions, where the transparency index must be re-mapped.
 */

static BOOL
filter_extension (
	const int	*imap,
	FILE		*infp,
	FILE		*outfp
) {
	unsigned char	c;
	BOOL		gif89_flag = FALSE;

	if (fread (&c, sizeof (char), 1, infp) != 1) {
	    fprintf (stderr, "Error: could not read extension code.\n");
	    return (FALSE);
	}
	if (fwrite (&c, sizeof (char), 1, outfp) != 1) {
	    perror (NULL);
	    return (FALSE);
	}

	if (c == 0xf9)
	    gif89_flag = TRUE;

	if (fread (&c, sizeof (char), 1, infp) != 1) {
	    fprintf (stderr, "Error: could not read data block size.\n");
	    return (FALSE);
	}
	if (fwrite (&c, sizeof (char), 1, outfp) != 1) {
	    perror (NULL);
	    return (FALSE);
	}

	while (c != 0) {
	    unsigned char	buf[256];

	    if (fread (buf, sizeof (char), c, infp) != c) {
		fprintf (stderr, "Error: could not read data block.\n");
		return (FALSE);
	    }

	    if (gif89_flag && (buf[0] & 1) != 0)
		buf[3] = imap[buf[3]];

	    if (fwrite (buf, sizeof (char), c, outfp) != c) {
		perror (NULL);
		return (FALSE);
	    }

	    if (fread (&c, sizeof (char), 1, infp) != 1) {
		fprintf (stderr, "Error: could not read data block size.\n");
		return (FALSE);
	    }
	    if (fwrite (&c, sizeof (char), 1, outfp) != 1) {
		perror (NULL);
		return (FALSE);
	    }
	}

	return (TRUE);
}


/*
 * Get a data block.
 */

static int
get_data_block (
	FILE		*fp,
	unsigned char	*buf
) {
	unsigned char	count;

	if (fread (&count, sizeof (char), 1, fp) != 1) {
	    fprintf (stderr, "Error: could not read data block size.\n");
	    return (-1);
	}

	if (count > 0 && fread (buf, sizeof (char), count, fp) != count) {
	    fprintf (stderr, "Error: could not read data block.\n");
	    return (-1);
	}

	if (block_size == 0)
	    block_size = count;

	return (count);
}


/*
 * Structures for storing LWZ parameters.
 */

#define MAX_LWZ_BITS	12

typedef struct {
	BOOL	lp_fresh;
	int	lp_code_size;
	int	lp_set_code_size;
	int	lp_max_code;
	int	lp_max_code_size;
	int	lp_first_code;
	int	lp_old_code;
	int	lp_clear_code;
	int	lp_end_code;
	int	lp_table[2][1 << MAX_LWZ_BITS];
	int	lp_stack[2 << MAX_LWZ_BITS];
	int	*lp_sp;
} LWZ_PARAMS;

typedef struct {
	unsigned char	lb_buffer[280];
	int		lb_curr_bit;
	int		lb_last_bit;
	int		lb_last_byte;
	BOOL		lb_done;
	BOOL		lb_zero_data_block;
} LWZ_BUFFER;


/*
 * Get an LWZ code.
 */

static int
get_lwz_code (
	FILE		*fp,
	int		code_size,
	LWZ_BUFFER	*lb
) {
	int		i, j, ret;

	if (lb->lb_curr_bit + code_size >= lb->lb_last_bit) {
	    int		count;

	    if (lb->lb_done) {
		if (lb->lb_curr_bit >= lb->lb_last_bit) {
		    fprintf (stderr, "Error: uncompression exceeded end.\n");
		    return (-2);
		}
		return (-1);
	    }

	    if (lb->lb_last_byte >= 2) {
	        lb->lb_buffer[0] = lb->lb_buffer[lb->lb_last_byte - 2];
	        lb->lb_buffer[1] = lb->lb_buffer[lb->lb_last_byte - 1];
	    }

	    count = get_data_block (fp, &lb->lb_buffer[2]);
	    lb->lb_zero_data_block = (count == 0);

	    if (count < 0)
		return (-2);
	    else if (count == 0)
		lb->lb_done = TRUE;

	    lb->lb_last_byte = count + 2;
	    lb->lb_curr_bit += 16 - lb->lb_last_bit;
	    lb->lb_last_bit = lb->lb_last_byte * 8;
	}

	ret = 0;
	i = lb->lb_curr_bit;
	for (j = 0; j < code_size; i++, j++)
	    if ((lb->lb_buffer[i / 8] & (1 << (i % 8))) != 0)
		ret |= (1 << j);

	lb->lb_curr_bit += code_size;

	return (ret);
}


/*
 * Uncompress a byte from the image.
 */

static int
lwz_read_byte (
	FILE		*fp,
	int		input_code_size,
	LWZ_PARAMS	*lp,
	LWZ_BUFFER	*lb
) {
	int		code, incode;
	int		i;

	if (lp->lp_fresh) {
	    lp->lp_fresh = FALSE;
	    do {
		lp->lp_old_code = get_lwz_code (fp, lp->lp_code_size, lb);
		lp->lp_first_code = lp->lp_old_code;
	    } while (lp->lp_first_code == lp->lp_clear_code);

	    return (lp->lp_first_code);
	}

	if (lp->lp_sp > lp->lp_stack)
	    return (*--lp->lp_sp);

	while ((code = get_lwz_code (fp, lp->lp_code_size, lb)) >= 0) {
	    if (code == lp->lp_clear_code) {
		if (max_code >= clear_code_index)
		    clear_code_index = max_code + 1;

		for (i=0; i<code; i++) {
		    lp->lp_table[0][i] = 0;
		    lp->lp_table[1][i] = i;
		}
		for (; i < (1 << MAX_LWZ_BITS); i++)
		    lp->lp_table[0][i] = lp->lp_table[1][i] = 0;

		lp->lp_code_size = lp->lp_set_code_size + 1;
		lp->lp_max_code_size = lp->lp_clear_code * 2;
		lp->lp_max_code = lp->lp_clear_code + 2;
		lp->lp_sp = lp->lp_stack;

		lp->lp_old_code = get_lwz_code (fp, lp->lp_code_size, lb);
		lp->lp_first_code = lp->lp_old_code;

		return (lp->lp_first_code);
	    } else if (code == lp->lp_end_code) {
		int		count;
		unsigned char	buf[260];

		use_end_code = TRUE;

		if (lb->lb_zero_data_block)
		    return (-2);

		while ((count = get_data_block (fp, buf)) > 0)
		    ;

		if (count != 0)
		    fprintf (stderr, "Error: missing EOD in data stream.\n");

		return (-2);
	    }

	    incode = code;

	    if (code >= lp->lp_max_code) {
		*lp->lp_sp++ = lp->lp_first_code;
		code = lp->lp_old_code;
	    }

	    while (code >= lp->lp_clear_code) {
		*lp->lp_sp++ = lp->lp_table[1][code];
		if (code == lp->lp_table[0][code]) {
		    fprintf (stderr, "Error: circular table entry.\n");
		    return (-2);
		}

		code = lp->lp_table[0][code];
	    }

	    *lp->lp_sp++ = lp->lp_first_code = lp->lp_table[1][code];

	    if ((code = lp->lp_max_code) < (1 << MAX_LWZ_BITS)) {
		lp->lp_table[0][code] = lp->lp_old_code;
		lp->lp_table[1][code] = lp->lp_first_code;

		lp->lp_max_code++;
		if (lp->lp_max_code >= lp->lp_max_code_size
			&& lp->lp_max_code_size < (1 << MAX_LWZ_BITS)) {
		    lp->lp_max_code_size *= 2;
		    lp->lp_code_size++;
		}

		if (code > max_code)
		    max_code = code;
	    }

	    lp->lp_old_code = incode;

	    if (lp->lp_sp > lp->lp_stack)
		return (*--lp->lp_sp);
	}

	return (code);
}


/*
 * Initialize the LWZ structure and buffer.
 */

static void
lwz_init (
	int		input_code_size,
	LWZ_PARAMS	*lp,
	LWZ_BUFFER	*lb
) {
	int		i;

	lp->lp_set_code_size = input_code_size;
	lp->lp_code_size = lp->lp_set_code_size + 1;
	lp->lp_clear_code = 1 << lp->lp_set_code_size;
	lp->lp_end_code = lp->lp_clear_code + 1;
	lp->lp_max_code_size = 2 * lp->lp_clear_code;
	lp->lp_max_code = lp->lp_clear_code + 2;
	lp->lp_fresh = TRUE;

	for (i=0; i<lp->lp_clear_code; i++) {
	    lp->lp_table[0][i] = 0;
	    lp->lp_table[1][i] = i;
	}
	for (; i < (1 << MAX_LWZ_BITS); i++)
	    lp->lp_table[0][i] = lp->lp_table[1][0] = 0;

	lp->lp_sp = lp->lp_stack;

	lb->lb_curr_bit = 0;
	lb->lb_last_bit = 0;
	lb->lb_last_byte = 0;
	lb->lb_done = FALSE;
	lb->lb_zero_data_block = FALSE;
}


/*
 * Load an image into the provided buffer.
 */

static BOOL
load_image (
	unsigned char	*image,
	int		size,
	FILE		*fp
) {
	unsigned char	c;
	int		i, v;
	LWZ_PARAMS	lwz;
	LWZ_BUFFER	lb;

		/* Initialise the compression routines */
	if (fread (&c, sizeof (char), 1, fp) != 1) {
	    fprintf (stderr, "Error: could not read image data.\n");
	    return (FALSE);
	}
	lwz_init (c, &lwz, &lb);

		/* Uncompress the image */
	i = 0;
	while ((v = lwz_read_byte (fp, c, &lwz, &lb)) >= 0) {
	    if (i >= size) {
		fprintf (stderr, "Error: too much image data.\n");
		return (FALSE);
	    }
	    image[i++] = v;
	}

	if (i < size) {
	    fprintf (stderr, "Error: incomplete image data.\n");
	    return (FALSE);
	}

	return (TRUE);
}


/*
 * The GIF encoding code is based on ppmtogif.c
 * As a result, I must include the following message.
 */
/* ppmtogif.c - read a portable pixmap and produce a GIF file
**
** Based on GIFENCOD by David Rowley <mgardi@watdscu.waterloo.edu>.A
** Lempel-Zim compression based on "compress".
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** The Graphics Interchange Format(c) is the Copyright property of
** CompuServe Incorporated.  GIF(sm) is a Service Mark property of
** CompuServe Incorporated.
*/

#define HSIZE		5003		/* 80% occupancy */

/*
 * Structure for handling compression params.
 */

typedef struct {
	int		cp_n_bits;	/* number of bits/code */
	long int	cp_maxcode;	/* maximum code, given n_bits */
	int		cp_free_ent;	/* first unused entry */
	BOOL		cp_clear_flag;
	int		cp_init_bits;
} COMPRESS_PARAMS;


/*
 * Structure for handling a 'packet'.
 */

typedef struct {
	int		p_count;
	unsigned char	p_buffer[256];
} PACKET;


/*
 * Structure for handling accumulated bits.
 */

typedef struct {
	unsigned long	ab_accum;
	int		ab_num_bits;
} ACCUM_BITS;


/*
 * Flush the packet to disk, and reset the accumulator
 */

static BOOL
packet_flush (
	PACKET		*p,
	FILE		*fp
) {
	if (p->p_count == 0)
	    return (TRUE);

	if (fputc (p->p_count, fp) < 0 || fwrite (p->p_buffer,
			sizeof (char), p->p_count, fp) != p->p_count) {
	    perror (NULL);
	    return (FALSE);
	}

	p->p_count = 0;

	return (TRUE);
}


/*
 * Add a character to the end of the current packet, and if it has block
 * size characters, flush the packet to disk.
 */

static BOOL
packet_write_char (
	PACKET		*p,
	int		block_size,
	unsigned char	c,
	FILE		*fp
) {
	p->p_buffer[p->p_count++] = c;
	if (p->p_count >= block_size)
	    return (packet_flush (p, fp));

	return (TRUE);
}


/*
 * Output the given code.
 */

static BOOL
output_code (
	int		code,
	ACCUM_BITS	*ab,
	PACKET		*p,
	COMPRESS_PARAMS *cp,
	FILE		*fp
) {
	static const unsigned long	masks[] = {
		0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f,
		0x007f, 0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff,
		0x3fff, 0x7fff, 0xffff};

	ab->ab_accum &= masks[ab->ab_num_bits];

	if (ab->ab_num_bits > 0)
	    ab->ab_accum |= code << ab->ab_num_bits;
	else
	    ab->ab_accum = code;

	ab->ab_num_bits += cp->cp_n_bits;

	while (ab->ab_num_bits >= 8) {
	    if (!packet_write_char (p, block_size, ab->ab_accum & 0xff, fp))
		return (FALSE);

	    ab->ab_accum >>= 8;
	    ab->ab_num_bits -= 8;
	}

		/*
		 * If the next entry is going to be too big for the code size,
		 * then increase it, if possible.
		 */
	if (cp->cp_free_ent > cp->cp_maxcode || cp->cp_clear_flag) {
	    if (cp->cp_clear_flag) {
		cp->cp_n_bits = cp->cp_init_bits;
		cp->cp_maxcode = (1 << cp->cp_n_bits) - 1;
		cp->cp_clear_flag = FALSE;
	    } else {
		cp->cp_n_bits++;
		if (cp->cp_n_bits == MAX_LWZ_BITS)
		    cp->cp_maxcode = 1 << MAX_LWZ_BITS;
		else
		    cp->cp_maxcode = (1 << cp->cp_n_bits) - 1;
	    }
	}

	return (TRUE);
}


/*
 * Reset code table.
 */

static void
clear_hash (
	long int	*tab
) {
	int		i;

	for (i=0; i<HSIZE; i++)
	    tab[i] = -1;
}


/*
 * Compress and save the image.
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe gives way to
 * a faster exclusive-or manipulation.  Also do block compression with an
 * adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static BOOL
save_compressed_image (
	int			init_bits,
	const unsigned char	*image,
	long int		size,
	FILE			*fp
) {
	long int	htab[HSIZE];
	unsigned short	codetab[HSIZE];
	long int	fcode, image_idx = 0;
	int		ent, hshift, clear_code, eof_code;
	PACKET		p;
	ACCUM_BITS	ab;
	COMPRESS_PARAMS	cp;

				/* Set up the globals */
	cp.cp_init_bits = init_bits;
	cp.cp_clear_flag = FALSE;
	cp.cp_n_bits = init_bits;
	cp.cp_maxcode = (1 << init_bits) - 1;
	if (block_size == 0)
	    block_size = 254;
	if (clear_code_index == 0)
	    clear_code_index = 1 << MAX_LWZ_BITS;

	clear_code = 1 << (init_bits - 1);
	eof_code = clear_code + 1;
	cp.cp_free_ent = clear_code + 2;

	p.p_count = 0;		/* Initialize the packet */

	ab.ab_num_bits = 0;	/* Initialize the accumulated bits */
	ab.ab_accum = 0;

	hshift = 8;
	for (fcode = HSIZE; fcode < 65536; fcode <<= 1)
	    hshift--;

	clear_hash (htab);		/* Clear hash table */

	if (!output_code (clear_code, &ab, &p, &cp, fp))
	    return (FALSE);

	ent = image[image_idx++];

	while (image_idx < size) {
	    int		c = image[image_idx++];
	    int		i = (c << hshift) ^ ent;	/* XOR hashing */

	    fcode = (c << MAX_LWZ_BITS) + ent;
	    if (htab[i] == fcode) {
		ent = codetab[i];
		continue;
	    }

	    if (htab[i] >= 0) {			/* Occupied slot */
		int	disp = HSIZE - i;	/* Secondary hash */

		if (i == 0)
		    disp = 1;

		do {
		    i -= disp;
		    if (i < 0)
			i += HSIZE;

		    if (htab[i] == fcode)
			break;
		} while (htab[i] > 0);

		if (htab[i] == fcode) {
		    ent = codetab[i];
		    continue;
		}
	    }

	    if (!output_code (ent, &ab, &p, &cp, fp))
		return (FALSE);

	    ent = c;

	    if (cp.cp_free_ent < clear_code_index) {
		codetab[i] = cp.cp_free_ent++;	/* Add code to hashtable */
		htab[i] = fcode;
	    } else {				/* Clear the hashtable */
		clear_hash (htab);
		cp.cp_free_ent = clear_code + 2;
		cp.cp_clear_flag = TRUE;

		if (!output_code (clear_code, &ab, &p, &cp, fp))
		    return (FALSE);
	    }
	}

		/* Put out the final code */
	if (!output_code (ent, &ab, &p, &cp, fp))
	    return (FALSE);
	if (use_end_code && !output_code (eof_code, &ab, &p, &cp, fp))
	    return (FALSE);

			/* At EOF, write the rest of the buffer */
	while (ab.ab_num_bits > 0) {
	    if (!packet_write_char (&p, block_size, ab.ab_accum & 0xff, fp))
		return (FALSE);

	    ab.ab_accum >>= 8;
	    ab.ab_num_bits -= 8;
	}

	if (!packet_flush (&p, fp))
	    return (FALSE);

	return (TRUE);
}


/*
 * Compress and save the image.
 */

static BOOL
save_image (
	const unsigned char	*image,
	int			bpp,
	long int		size,
	FILE			*fp
) {
	unsigned char		init_code_size;

	if (bpp < 2)
	    init_code_size = 2;
	else
	    init_code_size = bpp;

	if (fputc (init_code_size, fp) < 0) {
	    perror (NULL);
	    return (FALSE);
	}

	if (!save_compressed_image (init_code_size + 1, image, size, fp))
	    return (FALSE);

		/* Write out a zero-length packet (to end the series) */
	if (fputc (0, fp) < 0) {
	    perror (NULL);
	    return (FALSE);
	}
	return (TRUE);
}


/*
 * Filter a GIF image.
 * This involves expanding it, re-mapping the colours, then compressing it.
 */

static BOOL
filter_image (
	const GIFINFO	*gi,
	const int	*imap,
	FILE		*infp,
	FILE		*outfp
) {
	unsigned char	buf[9], *image;
	BOOL		local_cmap;
	int		width, height;
	long int	i, size;
	int		bpp = gi->gi_bits_per_pixel;

	if (fread (buf, sizeof (char), 9, infp) != 9) {
	    fprintf (stderr, "Error: could not read image header.\n");
	    return (FALSE);
	}
	if (fwrite (buf, sizeof (char), 9, outfp) != 9) {
	    perror (NULL);
	    return (FALSE);
	}

	local_cmap = ((buf[8] & 0x80) != 0);

	if (local_cmap) {
	    int			n;
	    unsigned char	cmbuf[768];

	    bpp = (buf[8] & 7) + 1;
	    n = 1 << bpp;

	    if (fread (cmbuf, sizeof (char), n * 3, infp) != n * 3) {
		fprintf (stderr, "Error: could not read local colourmap.\n");
		return (FALSE);
	    }
	    if (fwrite (cmbuf, sizeof (char), n * 3, outfp) != n * 3) {
		perror (NULL);
		return (FALSE);
	    }
	}

	width = buf[4] | (buf[5] << 8);
	height = buf[6] | (buf[7] << 8);

	if (width == 0 || height == 0) {
	    fprintf (stderr, "Error: illegal image dimensions (%d x %d).\n",
							width, height);
	    return (FALSE);
	}

	size = width * height;

	if ((image = (unsigned char *) malloc (size)) == NULL) {
	    fprintf (stderr, "Error: memory allocation failure.\n");
	    return (FALSE);
	}

	if (!load_image (image, size, infp)) {
	    free (image);
	    return (FALSE);
	}

	if (!local_cmap) {
	    for (i=0; i<size; i++)
		image[i] = imap[image[i]];
	}

	return (save_image (image, bpp, size, outfp));
}


/*
 * Check if two RGB entries match.
 */

static BOOL
rgb_match (
	const RGB	*v1,
	const RGB	*v2
) {
	return (v1->r == v2->r && v1->g == v2->g && v1->b == v2->b);
}


/*
 * Save a GIF image, filtering the image from the input file.
 */

BOOL
gif_filter_save (
	const GIFINFO	*gi,
	FILE		*infp,
	FILE		*outfp
) {
	int		i, n = gi->gi_num_colours;
	int		cidx[256];
	unsigned char	buf[768];

		/* Create a cross-mapping table for the colours */
	for (i=0; i<n; i++) {
	    const RGB	*orig = &gi->gi_orig_colours[i];
	    int		j;

	    for (j=0; j<n; j++)
		if (rgb_match (orig, &gi->gi_colours[j])) {
		    cidx[i] = j;
		    break;
		}
	}

		/* Write the header and new colourmap */
	for (i=0; i<13; i++)
	    buf[i] = gi->gi_header[i];

	buf[11] = cidx[buf[11]];	/* Re-map the background */
	if (fwrite (buf, sizeof (char), 13, outfp) != 13) {
	    perror (NULL);
	    return (FALSE);
	}

	for (i=0; i<n; i++) {
	    buf[i*3] = gi->gi_colours[i].r;
	    buf[i*3 + 1] = gi->gi_colours[i].g;
	    buf[i*3 + 2] = gi->gi_colours[i].b;
	}

	if (fwrite (buf, sizeof (char), n * 3, outfp) != n * 3) {
	    perror (NULL);
	    return (FALSE);
	}

		/* Filter through the image data */
	for (;;) {
	    if (fread (buf, sizeof (char), 1, infp) != 1) {
		fprintf (stderr, "Error: could not read image data.\n");
		return (FALSE);
	    }
	    if (fwrite (buf, sizeof (char), 1, outfp) != 1) {
		perror (NULL);
		return (FALSE);
	    }

	    if (buf[0] == ';')
		break;

	    switch (buf[0]) {
		case '!':
		    if (!filter_extension (cidx, infp, outfp))
			return (FALSE);
		    break;
		case ',':
		    if (!filter_image (gi, cidx, infp, outfp))
			return (FALSE);
		    break;
		default:
		    fprintf (stderr, "Error: unknown start character 0x%02x\n",
							(int) buf[0]);
		    return (FALSE);
	    }
	}

	return (TRUE);
}
