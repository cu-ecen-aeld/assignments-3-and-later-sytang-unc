#include<stdlib.h>
#include<stdio.h>
#include<syslog.h>

int main(int argc, char* argv[]) {
    openlog(NULL, LOG_PERROR | LOG_PID, LOG_USER);

    if (argc < 3) {
        syslog(LOG_ERR, "Invalid argument count: %d, expected at least 2", argc);
        exit(EXIT_FAILURE);
    }

    char *writefile, *writestr;
    writefile = argv[1];
    writestr = argv[2];

    FILE *fi;
    fi = fopen(writefile, "a");

    if (!fi) {
        syslog(LOG_ERR, "File %s not found", writefile);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    int ret;
    ret = fprintf(fi, "%s\n", writestr);

    if (ret < 0) {
        syslog(LOG_ERR, "Unable to write to file %s", writefile);
        exit(EXIT_FAILURE);
    }

    fclose(fi);

    closelog();

    exit(EXIT_SUCCESS);
}