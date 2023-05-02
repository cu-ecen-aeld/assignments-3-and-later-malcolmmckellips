CC=gcc

all: i2clight i2cioctl
default: i2clight i2cioctl

i2clight: i2clight.o
	$(CROSS_COMPILE)$(CC) i2clight.o -o i2clight

i2clight.o: i2clight.c
	$(CROSS_COMPILE)$(CC) -g -c i2clight.c -o i2clight.o -Wall -Werror

i2cioctl: i2cioctl.o
	$(CROSS_COMPILE)$(CC) i2cioctl.o -o i2cioctl

i2cioctl.o: i2cioctl.c
	$(CROSS_COMPILE)$(CC) -g -c i2cioctl.c -o i2cioctl.o -Wall -Werror

clean:
	rm -f *.o i2clight i2cioctl *.elf *.map

