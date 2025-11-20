#include<syslog.h>
#include<string.h>
#include<stdbool.h>

#include "fifo.h"

int init_fifo(struct fifo *fifo) {
    char *buf = (char*) calloc(AESD_DEFAULT_LEN + 1, sizeof(char));
    if (!buf)
        return -1;
    fifo->buf = buf;
    fifo->size = 0;
    fifo->max = AESD_DEFAULT_LEN;

    fifo->head = 0;
    fifo->tail = 0;

    return 0;
}

void free_fifo(struct fifo *fifo) {
    free(fifo->buf);
}

static inline bool wraps(struct fifo *fifo) {
    return fifo->size && fifo->head > fifo->tail;
}

//how many chars can be read from head (inclusive) to end of buf
static inline size_t remaining(struct fifo *fifo) {
    return fifo->max - fifo->head;
}

static inline char *head_ptr(struct fifo *fifo) {
    return &fifo->buf[fifo->head];
}

static void copy_fifo(struct fifo *fifo, char *buf) {
    if (wraps(fifo)) {
        size_t rem;
        rem = remaining(fifo);
        strncpy(buf, head_ptr(fifo), rem);
        strncpy(buf + rem, fifo->buf, fifo->tail);
    }
    else
        strncpy(buf, head_ptr(fifo), fifo->size);
}

static int resize_fifo(struct fifo *fifo) {
    if (fifo->size < fifo->max) 
        return 0;
    fifo->max <<= 1;
    char *new_buf = (char*) calloc(fifo->max + 1, sizeof(char));
    if (!new_buf) {
        syslog(LOG_ERR, "Unable to allocate memory for lengthening fifo");
        return -1;
    }
    copy_fifo(fifo, new_buf);
    free(fifo->buf);
    fifo->buf = new_buf;
    return 1;
}

size_t put(char *buf, struct fifo *fifo, size_t n) {
    int i;
    for (i = 0; i < n; i++) {
        if (resize_fifo(fifo) == -1)
            return i;
        fifo->buf[fifo->tail++] = buf[i];
        fifo->tail %= fifo->max;
        fifo->size++;
    }
    return n;
}

size_t get(char *buf, struct fifo *fifo, size_t n) {
    return get_until(buf, fifo, n, '\0');    
}

size_t get_until(char *buf, struct fifo *fifo, size_t n, char c) {
    if (n > fifo->size)
        n = fifo->size;
    int i;
    for (i = 0; i < n; i++) {
        buf[i] = fifo->buf[fifo->head++];
        fifo->head %= fifo->max;
        fifo->size--;
        if (buf[i] == c)
            return i + 1;
    }
    return n;
}

size_t drop_tail_until(struct fifo *fifo, char c) {
    size_t ret;
    ret = 0;
    while (fifo->size && fifo->buf[fifo->tail] != c) {
        fifo->size--;
        if (!fifo->tail)
            fifo->tail = fifo->max;
        fifo->tail--;
        ret++;
    }
    return ret;
}