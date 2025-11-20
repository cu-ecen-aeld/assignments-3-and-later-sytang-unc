#ifndef LOOP_FLAG_H
#define LOOP_FLAG_H

#include<stdbool.h>

extern volatile bool loop_flag;

void handler(int sig);

#endif /* LOOP_FLAG_H */