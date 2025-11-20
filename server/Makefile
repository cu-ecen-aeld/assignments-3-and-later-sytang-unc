all: aesdsocket

aesdsocket: aesdsocket.c packet_buffer.c loop_flag.c fifo.o
	$(CROSS_COMPILE)$(CC) -o aesdsocket aesdsocket.c packet_buffer.c loop_flag.c fifo.o

fifo.o: fifo.c
	$(CROSS_COMPILE)$(CC) -c -o fifo.o fifo.c

.PHONY: clean

clean:
	-rm aesdsocket fifo.o