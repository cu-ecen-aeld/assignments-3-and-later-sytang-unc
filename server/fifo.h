#ifndef AESD_FIFO_H
#define AESD_FIFO_H

#include<stdlib.h>

#define AESD_DEFAULT_LEN 100

//dynamically sized circular FIFO
struct fifo {
    char *buf;
    size_t size;
    size_t max;

    //index of first location where data would leave queue
    size_t head;
    //index of first location where data would enter queue
    size_t tail;
};

int init_fifo(struct fifo *fifo);

void free_fifo(struct fifo *fifo);

size_t put(char *buf, struct fifo *fifo, size_t n);

size_t get(char *buf, struct fifo *fifo, size_t n);

size_t get_until(char *buf, struct fifo *fifo, size_t n, char c);

size_t drop_tail_until(struct fifo *fifo, char c);

#endif /* AESD_FIFO_H */