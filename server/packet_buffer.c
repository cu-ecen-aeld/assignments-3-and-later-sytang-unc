#include<syslog.h>
#include<unistd.h>

#include "packet_buffer.h"
#include "loop_flag.h"

int init_pb(struct packet_buffer *pb) {
    if (init_fifo(&pb->fifo) == -1)
        return -1;
    pb->buf[AESD_DEFAULT_LEN] = '\0';
    pb->return_cnt = 0;
    return 0;
}

void free_pb(struct packet_buffer *pb) {
    free_fifo(&pb->fifo);
}

static int consume_dropped_packet(int s_fd) {
    char c;
    ssize_t nr_r;

    c = '\0';
    while (loop_flag) {
        nr_r = read(s_fd, &c, sizeof(char));
        if (nr_r == -1) {
            if (loop_flag)
                syslog(LOG_ERR, "Failed to read from socket");
            return -1;
        }
        if (!nr_r || c == '\n')
            break;
    }
    return 0;
}

ssize_t read_pb(int s_fd, struct packet_buffer *pb) {
    ssize_t nr_r, nr_p;
    nr_r = read(s_fd, pb->buf, AESD_DEFAULT_LEN);
    if (nr_r == -1) {
        if (loop_flag)
            syslog(LOG_ERR, "Failed to read from socket");
        return -1;
    }
    int i;
    nr_p = put(pb->buf, &pb->fifo, nr_r);
    if (nr_p < nr_r) {
        syslog(LOG_ERR, "Dropped packet");
        consume_dropped_packet(s_fd);
        nr_p -= drop_tail_until(&pb->fifo, '\n');
    }
    for (i = 0; i < nr_p; i++)
        if (pb->buf[i] == '\n')
            pb->return_cnt++;
    return nr_p;
}

static int send_file(int s_fd, int f_fd, bool need_seek) {
    if (need_seek && lseek(f_fd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "Failed to seek to beginning of file");
        return -1;
    }

    char buf[AESD_DEFAULT_LEN];
    ssize_t nr_r;
    nr_r = 1;
    while (nr_r) {
        nr_r = read(f_fd, buf, AESD_DEFAULT_LEN);
        if (nr_r == -1) {
            syslog(LOG_ERR, "Failed to read from file");
            return -1;
        }
        if (write(s_fd, buf, nr_r) == -1) {
            syslog(LOG_ERR, "Failed to write to socket");
            return -1;
        }
    }
    return 0;
}

ssize_t write_pb(int f_fd, int s_fd, struct packet_buffer *pb, bool need_seek) {
    ssize_t nr_w;

    if (!pb->return_cnt)
        return 0;

    while (pb->return_cnt) {
        size_t nr_g;
        do {
            nr_g = get_until(pb->buf, &pb->fifo, AESD_DEFAULT_LEN, '\n');  
            if (write(f_fd, pb->buf, nr_g) == -1) {
                syslog(LOG_ERR, "Failed to write to file");
                return -1;
            }
            nr_w += nr_g;
        } while (pb->buf[nr_g - 1] != '\n');
        if (send_file(s_fd, f_fd, need_seek) == -1)
            return -1;
        pb->return_cnt--;
    }

    return nr_w;
}