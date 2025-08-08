#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

extern atomic_int logger_interrupt;

void* logger_func(void* arg);