#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/timerfd.h>
#include <stdint.h>

extern atomic_int processor_interrupt;

void* processor_func(void* arg);