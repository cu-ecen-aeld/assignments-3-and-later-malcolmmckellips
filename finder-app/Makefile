CC=gcc

all: writer

writer: writer.o
	$(CROSS_COMPILE)$(CC) writer.o -o writer

writer.o: writer.c
	$(CROSS_COMPILE)$(CC) -c writer.c -o writer.o

clean:
	rm -f *.o writer *.elf *.map
