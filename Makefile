.PHONY = all clean

CC = gcc
FLAGS = -Wall -g -Wextra -Wno-deprecated-declarations
LDFLAGS = -lssl -lcrypto

all: stnc

stnc: stnc.c
	$(CC) $(FLAGS) stnc.c -o stnc $(LDFLAGS)

clean:
	rm -f *.o stnc