#
# Rudimentary makefile for the gifshuffle steganography program.
# Written by Matthew Kwan - January 1998
#

CC =		gcc
CFLAGS =	-O -Wall

OBJ =		main.o encrypt.o ice.o compress.o encode.o epi.o gif.o

gifshuffle:	$(OBJ)
		$(CC) -o $@ $(OBJ)

clean:
		/bin/rm -f $(OBJ) gifshuffle
