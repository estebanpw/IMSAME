CC=gcc
CXX=g++
CFLAGS=-O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -Wall -DVERBOSE
BIN=../bin

all: IMSAME revcomp



IMSAME: IMSAME.c
	$(CC) $(CFLAGS) alignmentFunctions.c commonFunctions.c IMSAME.c -lpthread -lm -o $(BIN)/IMSAME
	
revcomp: reverseComplement.c
	$(CC) $(CFLAGS) commonFunctions.c reverseComplement.c -o $(BIN)/revComp


clean:
	rm -rf $(BIN)/IMSAME
