.PHONY = all clean

CC = gcc
FLAGS = -Wall -g

all: stnc

stnc: stnc.c
	$(CC) $(FLAGS) stnc.c -o stnc

clean:
	rm -f *.o stnc