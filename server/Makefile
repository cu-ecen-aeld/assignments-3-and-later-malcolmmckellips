CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)
default: $(TARGET)

$(TARGET):$(TARGET).c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)
clean:
	rm -f *.o $(TARGET) *.elf *.map
