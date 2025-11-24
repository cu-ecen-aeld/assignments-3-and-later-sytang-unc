#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include<stdbool.h>

#include "fifo.h"

struct packet_buffer {
    struct fifo fifo;
    size_t return_cnt;
    //fixed-size flat buffer to move stuff to and from fifo
    char buf[AESD_DEFAULT_LEN + 1];
};

int init_pb(struct packet_buffer *pb);

void free_pb(struct packet_buffer *pb);

ssize_t read_pb(int s_fd, struct packet_buffer *pb);

ssize_t write_pb(int f_fd, int s_fd, struct packet_buffer *pb, bool need_seek);

#endif /* PACKET_BUFFER_H */