all: aesdsocket

aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)$(CC) -o aesdsocket aesdsocket.c

.PHONY: clean

clean:
	-rm aesdsocket