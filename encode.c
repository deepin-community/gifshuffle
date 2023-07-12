/*
 * GIF colourmap encoding routines for the gifshuffle steganography program.
 *
 * Written by Matthew Kwan - January 1998
 */

#include "gifshuf.h"
#include "epi.h"
#include "gif.h"

#include <stdlib.h>


/*
 * An RGB with position information and possibly its encrypted ciphertext.
 */

typedef struct {
	int		pos;
	RGB		rgb;
	unsigned char	ctext[8];
} CMAP_INFO;


/*
 * Local variables used for encoding.
 */

static int	encode_bit_count;
static EPI	encode_bits;


/*
 * Compare two RGB values.
 */

static int
rgb_cmp (
	const RGB	*r1,
	const RGB	*r2
) {
	int		v1 = (r1->r << 16) | (r1->g << 8) | r1->b;
	int		v2 = (r2->r << 16) | (r2->g << 8) | r2->b;

	return (v1 - v2);
}


/*
 * Compare two colourmap info values.
 */

static int
cmap_cmp (
	const void	*p1,
	const void	*p2
) {
	return (rgb_cmp (&((CMAP_INFO *) p1)->rgb, &((CMAP_INFO *) p2)->rgb));
}


/*
 * Compare two colourmap info values by their encrypted values.
 */

static int
cmap_encrypt_cmp (
	const void	*p1,
	const void	*p2
) {
	const CMAP_INFO		*c1 = (const CMAP_INFO *) p1;
	const CMAP_INFO		*c2 = (const CMAP_INFO *) p2;
	unsigned int		i;

	for (i=0; i<8; i++)
	    if (c1->ctext[i] != c2->ctext[i])
		return ((int) c1->ctext[i] - (int) c2->ctext[i]);

	return (0);
}


/*
 * Return the unique colours from a colourmap.
 * Put the non-unique colours at the top of the array.
 */

static int
unique_colours (
	const RGB	*cols,
	int		ncols,
	CMAP_INFO	*ci_array
) {
	int		i, n = 0, top = 0;

	for (i=0; i<ncols; i++) {
	    int		j;
	    BOOL	unique = TRUE;
	    const RGB	*r = &cols[i];

	    for (j=0; j<i; j++)
		if (rgb_cmp (r, &cols[j]) == 0) {
		    unique = FALSE;
		    break;
		}

	    if (unique)
		ci_array[n++].rgb = cols[i];
	    else
		ci_array[ncols - ++top].rgb = cols[i];
	}

	return (n);
}


/*
 * Calculate the largest number that can be encoded in a GIF colourmap.
 * This is equal to (number-of-unique-colours)! - 1
 */

static void
colourmap_max_storage (
	const GIFINFO	*gi,
	EPI		*epi
) {
	int		i, ncols = 0;
	CMAP_INFO	dummy[256];

	ncols = unique_colours (gi->gi_colours, gi->gi_num_colours, dummy);

	epi_set (epi, 1);
	for (i=2; i<=ncols; i++)
	    epi_multiply (epi, i);

	epi_decrement (epi);
}


/*
 * Encode a colourmap with the specified value.
 */

static BOOL
colourmap_encode (
	GIFINFO		*gi,
	EPI		*epi
) {
	int		i, ncols = 0;
	CMAP_INFO	ci_array[256];

	ncols = unique_colours (gi->gi_colours, gi->gi_num_colours, ci_array);

	if (encrypting_colourmap ()) {
	    for (i=0; i<ncols; i++) {
		CMAP_INFO	*ci = &ci_array[i];

		encrypt_colour (ci->rgb.r, ci->rgb.g, ci->rgb.b, ci->ctext);
	    }
	    qsort (ci_array, ncols, sizeof (CMAP_INFO), cmap_encrypt_cmp);
	} else
	    qsort (ci_array, ncols, sizeof (CMAP_INFO), cmap_cmp);

	for (i=0; i<ncols; i++)
	    ci_array[ncols - i - 1].pos = epi_divide (epi, i + 1);

	if (epi->epi_high_bit > 0) {
	    fprintf (stderr, "Error: remainder of %d bits.\n",
							epi->epi_high_bit);
	    return (FALSE);
	}

	for (i=0; i<ncols; i++) {
	    CMAP_INFO	*ci = &ci_array[ncols - i - 1];
	    int		j, pos = ci->pos;

	    for (j=i; j>pos; j--)
		gi->gi_colours[j] = gi->gi_colours[j - 1];

	    gi->gi_colours[pos] = ci->rgb;
	}

		/* Pad out the rest of the colourmap. */
	for (; i<gi->gi_num_colours; i++)
	    gi->gi_colours[i] = gi->gi_colours[gi->gi_num_colours - 1];

	return (TRUE);
}


/*
 * Initialize the encoding routines.
 */

void
encode_init (void)
{
	encode_bit_count = 0;
	epi_init (&encode_bits);
}


/*
 * Encode a single bit.
 */

BOOL
encode_bit (
	int		bit,
	FILE		*inf,
	FILE		*outf
) {
	encode_bit_count++;
	if (encode_bits.epi_high_bit < EPI_MAX_BITS)
	    encode_bits.epi_bits[encode_bits.epi_high_bit++] = bit;

	return (TRUE);
}


/*
 * Flush the contents of the encoding routines.
 */

BOOL
encode_flush (
	FILE		*inf,
	FILE		*outf
) {
	GIFINFO		gi;
	EPI		max_epi;
	int		max_bits;

	encode_bit_count++;
	if (encode_bits.epi_high_bit < EPI_MAX_BITS)
	    encode_bits.epi_bits[encode_bits.epi_high_bit++] = 1;

	if (!gif_header_load (&gi, inf)) {
	    fprintf (stderr, "Input file is not in GIF format.\n");
	    return (FALSE);
	}

	colourmap_max_storage (&gi, &max_epi);
	max_bits = max_epi.epi_high_bit;

	if (encode_bit_count > max_epi.epi_high_bit
				|| epi_cmp (&encode_bits, &max_epi) > 0) {
	    if (max_bits == 0)
		fprintf (stderr, "GIF file has no storage space.\n");
	    else
		fprintf (stderr,
		"Message exceeded available space by approximately %.2f%%.\n",
			((double) encode_bit_count / max_bits - 1.0) * 100.0);

	    return (FALSE);
	}

	if (!colourmap_encode (&gi, &encode_bits))
	    return (FALSE);

	if (!gif_filter_save (&gi, inf, outf))
	    return (FALSE);

	if (!quiet_flag)
	    fprintf (stderr,
		"Message used approximately %.2f%% of available space.\n",
				(double) encode_bit_count / max_bits * 100.0);

	return (TRUE);
}


/*
 * Decode a value from a colourmap.
 */

static void
colourmap_decode (
	const GIFINFO	*gi,
	EPI		*epi
) {
	int		i, ncols = 0;
	CMAP_INFO	ci_array[256];

	ncols = unique_colours (gi->gi_colours, gi->gi_num_colours, ci_array);
	for (i=0; i<ncols; i++)
	    ci_array[i].pos = i;

	if (encrypting_colourmap ()) {
	    for (i=0; i<ncols; i++) {
		CMAP_INFO	*ci = &ci_array[i];

		encrypt_colour (ci->rgb.r, ci->rgb.g, ci->rgb.b, ci->ctext);
	    }
	    qsort (ci_array, ncols, sizeof (CMAP_INFO), cmap_encrypt_cmp);
	} else
	    qsort (ci_array, ncols, sizeof (CMAP_INFO), cmap_cmp);

	epi_init (epi);

	for (i = 0; i < ncols - 1; i++) {
	    int		j, pos = ci_array[i].pos;
	    EPI		epi_pos;

	    epi_multiply (epi, ncols - i);
	    epi_set (&epi_pos, pos);
	    epi_add (epi, &epi_pos);

	    for (j = i + 1; j < ncols; j++)
		if (ci_array[j].pos > pos)
		    ci_array[j].pos--;
	}
}


/*
 * Extract a message from the input stream.
 */

BOOL
message_extract (
	FILE		*inf,
	FILE		*outf
) {
	GIFINFO		gi;
	EPI		epi;
	int		i;

	decrypt_init ();

	if (!gif_header_load (&gi, inf)) {
	    fprintf (stderr, "Input file is not in GIF format.\n");
	    return (FALSE);
	}

	colourmap_decode (&gi, &epi);

	for (i = 0; i < epi.epi_high_bit - 1; i++)
	    if (!decrypt_bit (epi.epi_bits[i], outf))
		return (FALSE);

	return (decrypt_flush (outf));
}


/*
 * Calculate the amount of covert information that can be stored
 * in the file.
 */

void
space_calculate (
	FILE		*fp
) {
	GIFINFO		gi;
	EPI		max_epi;
	int		max_bits;

	if (!gif_header_load (&gi, fp)) {
	    fprintf (stderr, "Input file is not in GIF format.\n");
	    return;
	}

	colourmap_max_storage (&gi, &max_epi);
	max_bits = max_epi.epi_high_bit - 1;
	if (max_bits < 0)
	    max_bits = 0;
	printf ("File has storage capacity of %d bits (%d bytes)\n",
						max_bits, max_bits / 8);
}
