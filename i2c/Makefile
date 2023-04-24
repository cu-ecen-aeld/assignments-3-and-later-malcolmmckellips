CC=gcc

all: i2clight
default: i2clight

i2clight: i2clight.o
	$(CROSS_COMPILE)$(CC) i2clight.o -o i2clight

i2clight.o: i2clight.c
	$(CROSS_COMPILE)$(CC) -g -c i2clight.c -o i2clight.o -Wall -Werror

clean:
	rm -f *.o i2clight *.elf *.map

