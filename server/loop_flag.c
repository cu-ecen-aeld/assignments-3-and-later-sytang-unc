#include "loop_flag.h"

volatile bool loop_flag;

void handler(int sig) {
    loop_flag = false;
}