CROSS_COMPILE=
ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC=aarch64-none-linux-gnu-gcc
else
	CC=gcc
endif

writer: writer.c
	$(CC) -o writer writer.c

.PHONY: clean

clean:
	-rm writer