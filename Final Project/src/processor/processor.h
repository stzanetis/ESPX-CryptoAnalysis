#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

extern atomic_int processor_interrupt;

void* processor_func(void* arg);