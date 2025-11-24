#define _POSIX_C_SOURCE 200809L
#include<syslog.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<signal.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include<time.h>

#include "loop_flag.h"
#include "../aesd-char-driver/aesd_ioctl.h"

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif

#define AESD_DEFAULT_LEN 100

struct thread_arg {
    FILE *s_stream;
    struct sockaddr_in addr;
    pthread_t thread;
    struct thread_arg *next;
};

static struct thread_arg *completed = NULL;
static int waiting_on = 0;
static pthread_mutex_t comp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t comp_cond = PTHREAD_COND_INITIALIZER;

#ifndef USE_AESD_CHAR_DEVICE
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static int send_file(FILE *s_stream, int f_fd) {
    if (lseek(f_fd, 0, SEEK_SET) == -1) {
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
        fwrite(buf, sizeof(char), nr_r, s_stream);
    }
    return 0;
}

int process_packets(FILE *s_stream) {
    int f_fd, ret;
    size_t nr_r;
    char *line_ptr;
#ifdef USE_AESD_CHAR_DEVICE
    struct aesd_seekto st;
#endif

    ret = 0;

#ifdef USE_AESD_CHAR_DEVICE
    if ((f_fd = open("/dev/aesdchar", O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
        syslog(LOG_ERR, "Failed to open /dev/aesdchar");
#else
    if ((f_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
        syslog(LOG_ERR, "Failed to open /var/tmp/aesdsocketdata");
#endif
        ret = -1;
    }

    line_ptr = NULL;
    nr_r = 1;
    while (loop_flag && nr_r && ret != -1) {
        if (getline(&line_ptr, &nr_r, s_stream) == -1) {
            nr_r = 0;
            if (!feof(s_stream)) {
                syslog(LOG_ERR, "Failed to get line from socket stream");
                ret = -1;
                continue;
            }
        }
#ifndef USE_AESD_CHAR_DEVICE
        if (pthread_mutex_lock(&file_mutex)) {
            syslog(LOG_ERR, "Failed to lock file mutex for writing");
            ret = -1;
            continue;
        }
#else
        if (sscanf(line_ptr, "AESDCHAR_IOCSEEKTO:%u,%u", &st.write_cmd, &st.write_cmd_offset) == 2) {
            if (ioctl(f_fd, AESDCHAR_IOCSEEKTO, &st) == -1){
                syslog(LOG_ERR, "Failed ioctl");
                ret = -1;
                continue;
            }       
        }
        else
#endif
        if (write(f_fd, line_ptr, nr_r) == -1) {
            syslog(LOG_ERR, "Failed to write to file");
            ret = -1;
            goto unlock_pp;
        }
        
        if (send_file(s_stream, f_fd) == -1) {
            ret = -1;
            goto unlock_pp;
        }

unlock_pp:
#ifndef USE_AESD_CHAR_DEVICE
        if (pthread_mutex_unlock(&file_mutex)) {
            syslog(LOG_ERR, "Failed to unlock file mutex after writing");
            ret = -1;
        }
#endif
        free(line_ptr);
        line_ptr = NULL;
    }

    close(f_fd);

    return ret;
}

void *do_process_packets(void *arg) {
    struct thread_arg *ta = (struct thread_arg*) arg;
    process_packets(ta->s_stream);

    syslog(LOG_USER, "Closed connection from %s", inet_ntoa(ta->addr.sin_addr));
    fclose(ta->s_stream);

    if (pthread_mutex_lock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to lock completion mutex on thread completion");
        return NULL;
    }
    ta->next = completed;
    completed = ta;
    if (pthread_cond_signal(&comp_cond)) {
        syslog(LOG_ERR, "Failed to signal thread completion");
        return NULL;
    }
    if (pthread_mutex_unlock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to unlock completion mutex on thread completion");
        return NULL;
    }

    return NULL;
}

void *do_join_complete(void *arg) {
    if (pthread_mutex_lock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to acquire completion mutex in completion thread");
        return NULL;
    }
    while (loop_flag || waiting_on) {
        if (pthread_cond_wait(&comp_cond, &comp_mutex)) {
            syslog(LOG_ERR, "Failed to wait on condition variable");
            return NULL;
        }
        while (completed) {
            struct thread_arg *next;
            if (pthread_join(completed->thread, NULL)) {
                syslog(LOG_ERR, "Failed to join pthread");
                return NULL;
            }
            next = completed->next;
            free(completed);
            completed = next;
            waiting_on--;
        }
    }
    if (pthread_mutex_unlock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to release completion mutex in completion thread");
        return NULL;
    }

    return NULL;
}


#ifndef USE_AESD_CHAR_DEVICE
void *do_print_ts(void *arg) {
    char t_str[100];
    time_t t;
    struct tm *tmp;
    size_t nr_t;
    struct timespec sleep_time;

    sleep_time = (struct timespec) {.tv_sec = 10};

    while (loop_flag) {
        t = time(NULL);
        tmp = localtime(&t);
        if (tmp == NULL) {
            syslog(LOG_ERR, "Failed to get localtime");
            return NULL;
        }

        nr_t = strftime(t_str, sizeof(t_str), "timestamp:%a, %d %b %Y %T %z\n", tmp);

        if (pthread_mutex_lock(&file_mutex)) {
            syslog(LOG_ERR, "Failed to lock file mutex to write timestamp");
            return NULL;
        }
        if (write(f_fd, t_str, nr_t) == -1) {
            syslog(LOG_ERR, "Failed to write timestamp");
            return NULL;
        }
        if (pthread_mutex_unlock(&file_mutex)) {
            syslog(LOG_ERR, "Failed to unlock file mutex after writing timestamp");
            return NULL;
        }

        nanosleep(&sleep_time, NULL);
    }
    return NULL;
}
#endif

int do_aesdsocket(int socket_fd) {
    pthread_t comp_thread;
    if (pthread_create(&comp_thread, NULL, do_join_complete, NULL)) {
        syslog(LOG_ERR, "Failed to create completion cleanup thread");
        return -1;
    }

#ifndef USE_AESD_CHAR_DEVICE
    pthread_t ts_thread;
    if (pthread_create(&ts_thread, NULL, do_print_ts, NULL)) {
        syslog(LOG_ERR, "Failed ot create ts printing thread");
        return -1;
    }
#endif

    if (listen(socket_fd, 5) == -1){
        syslog(LOG_ERR, "Failed to listen on socket");
        return -1;
    }

    int s_fd;
    socklen_t addr_len;

    while (loop_flag) {
        struct thread_arg *ta;
        ta = (struct thread_arg*) malloc(sizeof(struct thread_arg));
        if (!ta) {
            syslog(LOG_ERR, "Failed to allocate memory for thread_arg");
            return -1;
        }


        addr_len = sizeof(struct sockaddr_in);
        s_fd = accept(socket_fd, (struct sockaddr*) &ta->addr, &addr_len);
        if (s_fd == -1) {
            if (errno == EINTR) {
                free(ta);
                continue;
            }
            syslog(LOG_ERR, "Failed to accept socket connection");
            free(ta);
            return -1;
        }
        syslog(LOG_USER, "Accepted connection from %s", inet_ntoa(ta->addr.sin_addr));
        ta->s_stream = fdopen(s_fd, "r+");
        if (!ta->s_stream) {
            syslog(LOG_ERR, "Failed to open socket stream");
            return -1;
        }
        ta->next = NULL;

        if (pthread_mutex_lock(&comp_mutex)) {
            syslog(LOG_ERR, "Failed to lock completion mutex when creating thread");
            return -1;
        }
        if (pthread_create(&ta->thread, NULL, do_process_packets, ta)) {
            syslog(LOG_ERR, "Failed to create pthread");
            free(ta);
            return -1;
        }
        waiting_on++;
        if (pthread_mutex_unlock(&comp_mutex)) {
            syslog(LOG_ERR, "Failed to unlock completion mutex when creating thread");
            return -1;
        }
    }

    if (pthread_mutex_lock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to lock completion mutex after TERM received");
        return -1;
    }
    if (pthread_cond_signal(&comp_cond)) {
        syslog(LOG_ERR, "Failed to signal completion thread after TERM received");
        return -1;
    }
    if (pthread_mutex_unlock(&comp_mutex)) {
        syslog(LOG_ERR, "Failed to unlock completion mutex after TERM received");
        return -1;
    }

    if (pthread_join(comp_thread, NULL)) {
        syslog(LOG_ERR, "Failed to join completion thread");
        return -1;
    }

#ifndef USE_AESD_CHAR_DEVICE
    //Interrupt the timestamp thread in case it's mid nanosleep
    //May unset loop_flag again but that should be fine
    if (pthread_kill(ts_thread, SIGTERM)) {
        syslog(LOG_ERR, "Failed to signal timestamp thread to stop");
        return -1;
    }
    if (pthread_join(ts_thread, NULL)) {
        syslog(LOG_ERR, "Failed to join timestamp thread");
        return -1;
    }

    if (remove("/var/tmp/aesdsocketdata") == -1){
        syslog(LOG_ERR, "Failed to remove tmp file");
        return -1;
    }
#endif

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