all: writer

writer: writer.o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(INCLUDES) writer.o -o writer $(LDFLAGS)

writer.o: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(INCLUDES) -c writer.c -o writer.o $(LDFLAGS)

clean:
	rm -f *.o writer *.elf *.map
