CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -g -Wall -Werror

TARGET ?= aesdsocket

LDFLAGS ?= -lpthread -lrt

all: $(TARGET)

$(TARGET): aesdsocket.c loop_flag.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

.PHONY: clean

clean:
	-rm $(TARGET)