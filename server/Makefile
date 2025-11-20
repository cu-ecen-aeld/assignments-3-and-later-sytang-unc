CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -g -Wall -Werror

TARGET ?= aesdsocket

LDFLAGS ?= -lpthread -lrt

all: $(TARGET)

$(TARGET): aesdsocket.c packet_buffer.c loop_flag.c fifo.o
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

fifo.o: fifo.c
	$(CC) $(CFLAGS) $^ -c -o $@

.PHONY: clean

clean:
	-rm $(TARGET) fifo.o