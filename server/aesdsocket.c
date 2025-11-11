#define _POSIX_C_SOURCE 200809L
#include<syslog.h>
#include<unistd.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<errno.h>
#include<signal.h>
#include<fcntl.h>
#include<string.h>

volatile bool loop_flag;

void handler(int sig) {
    loop_flag = false;
}

#define AESD_BUFF 100

bool send_file(int s_fd, int f_fd) {
    if (lseek(f_fd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "Failed to seek file");
        return false;
    }

    char buf[AESD_BUFF];
    ssize_t nr_r;
    nr_r = 1;
    while (nr_r) {
        nr_r = read(f_fd, buf, AESD_BUFF);
        if (nr_r == -1) {
            syslog(LOG_ERR, "Failed to read from file");
            return false;
        }
        if (write(s_fd, buf, nr_r) == -1) {
            syslog(LOG_ERR, "Failed to write to socket");
            return false;
        }
    }
    return true;
}

bool process_packets(int s_fd, int f_fd) {
    char buf[AESD_BUFF + 1];
    ssize_t nr_r;

    nr_r = 1;
    while (loop_flag && nr_r) {
        nr_r = read(s_fd, buf, AESD_BUFF);
        if (nr_r == -1) {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "Failed to read from socket");
            return false;
        }
        buf[nr_r] = '\0';
        if (nr_r) {
            char *begin, *packet_end;
            begin = buf;
            packet_end = strchr(begin, '\n');
            while (packet_end) {
                packet_end++;
                if (write(f_fd, begin, (packet_end - begin)) == -1) {
                    syslog(LOG_ERR, "Failed to write to file");
                    return false;
                }
                if (!send_file(s_fd, f_fd))
                    return false;
                begin = packet_end;
                packet_end = strchr(begin, '\n');
            }
            if (*begin)
                if (write(f_fd, begin, nr_r - (begin - buf)) == -1) {
                    syslog(LOG_ERR, "Failed to write to file");
                    return false;
                }
        }
    }
    return true;
}

int do_aesdsocket(int socket_fd) {
    int a_fd;
    if ((a_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
        syslog(LOG_ERR, "Failed to open /var/tmp/aesdsocketdata");
        return -1;
    }

    if (listen(socket_fd, 5) == -1){
        syslog(LOG_ERR, "Failed to listen on socket");
        return -1;
    }

    int s_fd;
    struct sockaddr_in addr;
    socklen_t addr_len;

    while (loop_flag) {
        addr_len = sizeof(struct sockaddr_in);
        s_fd = accept(socket_fd, (struct sockaddr*) &addr, &addr_len);
        if (s_fd == -1) {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "Failed to accept socket connection");
            return -1;
        }
        syslog(LOG_USER, "Accepted connection from %s", inet_ntoa(addr.sin_addr));
        process_packets(s_fd, a_fd);
        syslog(LOG_USER, "Closed connection from %s", inet_ntoa(addr.sin_addr));
        close(s_fd);
    }

    close(a_fd);

    if (remove("/var/tmp/aesdsocketdata") == -1){
        syslog(LOG_ERR, "Failed to remove tmp file");
        return -1;
    }

    return 0;
}

int make_daemon(int socket_fd) {
    pid_t pid;
    if ((pid = fork()) == -1) {
        syslog(LOG_ERR, "Failed to fork daemon process");
        return -1;
    }
    if (!pid) {
        if (setsid() == -1){
            syslog(LOG_ERR, "Failed to create new session");
            return -1;
        }
        return do_aesdsocket(socket_fd);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    bool daemon;
    int opt_int, ret;

    ret = -1;

    daemon = false;
    while ((opt_int = getopt(argc, argv, "d")) != -1)
        if (((char) opt_int) == 'd')
            daemon = true;
    
    openlog(NULL, LOG_PERROR | LOG_PID, LOG_USER);

    struct sigaction action;
    action = (struct sigaction) {
        .sa_handler = handler
    };
    if (sigaction(SIGINT, &action, NULL) == -1){
        syslog(LOG_ERR, "Failed to register handler for SIGINT");
        goto close_log;
    }
    if (sigaction(SIGTERM, &action, NULL) == -1) {
        syslog(LOG_ERR, "Failed to register handler for SIGTERM");
        goto close_log;
    }

    int s_fd;
    if ((s_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        syslog(LOG_ERR, "Failed to create socket");
        goto close_log;
    }

    struct addrinfo hints, *res;
    hints = (struct addrinfo) {
        .ai_family = AF_INET, 
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };
    if (getaddrinfo(NULL, "9000", &hints, &res) == -1) {
        syslog(LOG_ERR, "Failed to get address info");
        goto close_socket;
    }

    if (bind(s_fd, res->ai_addr, res->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Failed to bind socket");
        goto free_addr;
    }

    loop_flag = true;
    ret = daemon? make_daemon(s_fd) : do_aesdsocket(s_fd);

free_addr:
    freeaddrinfo(res);    
close_socket:
    close(s_fd);
close_log:
    closelog();
    
    return ret;
}