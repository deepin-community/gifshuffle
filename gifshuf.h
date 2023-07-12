/*
 * Header file for the gifshuffle steganography program.
 *
 * Written by Matthew Kwan - January 1998
 */

#ifndef _GIFSHUF_H
#define _GIFSHUF_H

#include <stdio.h>


/*
 * Define boolean types.
 */

typedef int	BOOL;

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif


/*
 * Define global variables.
 */

extern BOOL	compress_flag;
extern BOOL	quiet_flag;
extern BOOL	v1_flag;


/*
 * Define external functions.
 */

extern void	password_set (const char *passwd);
extern BOOL	encrypting_colourmap ();
extern void	encrypt_colour (unsigned char r, unsigned char g,
				unsigned char b, unsigned char *ctext);

extern BOOL	message_extract (FILE *inf, FILE *outf);
extern void	space_calculate (FILE *inf);

extern void	compress_init (void);
extern BOOL	compress_bit (int bit, FILE *inf, FILE *outf);
extern BOOL	compress_flush (FILE *inf, FILE *outf);

extern void	uncompress_init (void);
extern BOOL	uncompress_bit (int bit, FILE *outf);
extern BOOL	uncompress_flush (FILE *outf);

extern void	encrypt_init (void);
extern BOOL	encrypt_bit (int bit, FILE *inf, FILE *outf);
extern BOOL	encrypt_flush (FILE *inf, FILE *outf);

extern void	decrypt_init (void);
extern BOOL	decrypt_bit (int bit, FILE *outf);
extern BOOL	decrypt_flush (FILE *outf);

extern void	encode_init (void);
extern BOOL	encode_bit (int bit, FILE *inf, FILE *outf);
extern BOOL	encode_flush (FILE *inf, FILE *outf);

#endif
