.PHONY = all clean

CC = gcc
FLAGS = -Wall -g -Wextra -Wno-deprecated-declarations
LDFLAGS = -lssl -lcrypto
LRTFLAG = -lrt

all: stnc

stnc: stnc.c
	$(CC) $(FLAGS) stnc.c -o stnc $(LDFLAGS) $(LRTFLAG)

clean:
	rm -f *.o stnc