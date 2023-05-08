.PHONY = all clean

CC = gcc
FLAGS = -Wall -g
LDFLAGS = -lssl -lcrypto

all: stnc

stnc: stnc.c
	$(CC) $(FLAGS) stnc.c -o stnc $(LDFLAGS)

clean:
	rm -f *.o stnc