/*
 * Command-line program for hiding and extracting messages within
 * the colourmap of GIF images.
 *
 * Usage: gifshuffle [-C][-Q][-S][-1][-p passwd] [-f file | -m message]
 *					[infile [outfile]]
 *
 *	-C : Use compression
 *	-Q : Be quiet
 *	-S : Calculate the space available in the file
 *	-1 : Use the old Gifshuffle 1.0 concealment algorithm
 *	-p : Specify the password to encrypt the message
 *
 *	-f : Insert the message contained in the file
 *	-m : Insert the message given
 *
 * If the program is executed without either of the -f or -m options
 * then the program will attempt to extract a concealed message.
 * The output will go to outfile if specified, stdout otherwise.
 *
 * Written by Matthew Kwan - January 1998
 */

#include "gifshuf.h"


/*
 * Declaration of global variables.
 */

BOOL	compress_flag = FALSE;
BOOL	quiet_flag = FALSE;
BOOL	v1_flag = FALSE;


/*
 * Encode a single character.
 */

static BOOL
character_encode (
	unsigned char	c,
	FILE		*infile,
	FILE		*outfile
) {
	int		i;

	for (i=0; i<8; i++) {
	    int		bit = ((c & (128 >> i)) != 0) ? 1 : 0;

	    if (!compress_bit (bit, infile, outfile))
		return (FALSE);
	}

	return (TRUE);
}


/*
 * Encode a string of characters.
 */

static BOOL
message_string_encode (
	const char	*msg,
	FILE		*infile,
	FILE		*outfile
) {
	compress_init ();

	while (*msg != '\0') {
	    if (!character_encode (*msg, infile, outfile))
		return (FALSE);
	    msg++;
	}

	return (compress_flush (infile, outfile));
}


/*
 * Encode the contents of a file.
 */

static BOOL
message_fp_encode (
	FILE		*msg_fp,
	FILE		*infile,
	FILE		*outfile
) {
	int		c;

	compress_init ();

	while ((c = fgetc (msg_fp)) != EOF)
	    if (!character_encode (c, infile, outfile))
		return (FALSE);

	if (ferror (msg_fp) != 0) {
	    perror ("Message file");
	    return (FALSE);
	}

	return (compress_flush (infile, outfile));
}


/*
 * Program's starting point.
 * Processes command-line args and starts things running.
 */

int
main (
	int		argc,
	char		*argv[]
) {
	int		optind;
	BOOL		errflag = FALSE;
	BOOL		space_flag = FALSE;
	char		*passwd = NULL;
	char		*message_string = NULL;
	FILE		*message_fp = NULL;
	FILE		*infile = stdin;
	FILE		*outfile = stdout;

	optind = 1;
	for (optind = 1; optind < argc
#ifdef unix
			&& argv[optind][0] == '-';
#else
			&& (argv[optind][0] == '-' || argv[optind][0] == '/');
#endif
						optind++) {
	    char	c = argv[optind][1];
	    char	*optarg;

	    switch (c) {
		case 'C':
		    compress_flag = TRUE;
		    break;
		case 'Q':
		    quiet_flag = TRUE;
		    break;
		case 'S':
		    space_flag = TRUE;
		    break;
		case '1':
		    v1_flag = TRUE;
		    break;
		case 'f':
		    if (argv[optind][2] != '\0')
			optarg = &argv[optind][2];
		    else if (++optind == argc) {
			errflag = TRUE;
			break;
		    } else
			optarg = argv[optind];

		    if ((message_fp = fopen (optarg, "rb")) == NULL) {
			perror (optarg);
			errflag = TRUE;
		    }
		    break;
		case 'm':
		    if (argv[optind][2] != '\0')
			optarg = &argv[optind][2];
		    else if (++optind == argc) {
			errflag = TRUE;
			break;
		    } else
			optarg = argv[optind];

		    message_string = optarg;
		    break;
		case 'p':
		    if (argv[optind][2] != '\0')
			optarg = &argv[optind][2];
		    else if (++optind == argc) {
			errflag = TRUE;
			break;
		    } else
			optarg = argv[optind];

		    passwd = optarg;
		    break;
		default:
		    fprintf (stderr, "Illegal option '%s'\n", argv[optind]);
		    errflag = TRUE;
		    break;
	    }

	    if (errflag)
		break;
	}

	if (message_string != NULL && message_fp != NULL) {
	    fprintf (stderr, "Cannot specify both message string and file\n");
	    errflag = TRUE;
	}

	if (errflag || optind < argc - 2) {
	    fprintf (stderr, "Usage: %s [-C][-Q][-S][-1] ", argv[0]);
	    fprintf (stderr, "[-p passwd] [-f file | -m message]\n");
	    fprintf (stderr, "\t\t\t\t\t[infile [outfile]]\n");
	    return (1);
	}

	if (passwd != NULL)
	    password_set (passwd);

	if (optind < argc) {
	    if ((infile = fopen (argv[optind], "rb")) == NULL) {
		perror (argv[optind]);
		return (1);
	    }
	}

	if (optind + 1 < argc) {
	    if ((outfile = fopen (argv[optind + 1], "wb")) == NULL) {
		perror (argv[optind + 1]);
		return (1);
	    }
	}

	if (space_flag) {
	    space_calculate (infile);
	} else if (message_string != NULL) {
	    if (!message_string_encode (message_string, infile, outfile))
		return (1);
	} else if (message_fp != NULL) {
	    if (!message_fp_encode (message_fp, infile, outfile))
		return (1);
	    fclose (message_fp);
	} else {
	    if (!message_extract (infile, outfile))
		return (1);
	}

	if (outfile != stdout)
	    fclose (outfile);
	if (infile != stdout)
	    fclose (infile);

	return (0);
}
