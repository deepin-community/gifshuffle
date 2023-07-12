/*
 * Extended-precision integers.
 *
 * Written by Matthew Kwan - January 1998
 */

#ifndef _EPI_H
#define _EPI_H

#define EPI_MAX_BITS	2040

typedef struct {
	int	epi_high_bit;
	char	epi_bits[EPI_MAX_BITS];
} EPI;


/*
 * Define external functions.
 */

extern void	epi_init (EPI *epi);
extern void	epi_set (EPI *epi, int n);
extern int	epi_cmp (const EPI *epi1, const EPI *epi2);
extern void	epi_add (EPI *epi1, const EPI *epi2);
extern void	epi_multiply (EPI *epi, int n);
extern int	epi_divide (EPI *epi, int n);
extern void	epi_decrement (EPI *epi);

#endif
