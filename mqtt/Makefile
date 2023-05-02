CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= mqtt
LDFLAGS ?= -lpaho-mqtt3cs

all: $(TARGET)
default: $(TARGET)

$(TARGET):$(TARGET).c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)
clean:
	rm -f *.o $(TARGET) *.elf *.map
	
