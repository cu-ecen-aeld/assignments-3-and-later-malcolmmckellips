CC=gcc

all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.o
	$(CROSS_COMPILE)$(CC) aesdsocket.o -o aesdsocket -lpthread

aesdsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) -g -c aesdsocket.c -o aesdsocket.o -Wall -Werror

clean:
	rm -f *.o aesdsocket *.elf *.map

