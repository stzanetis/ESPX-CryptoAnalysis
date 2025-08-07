#include "logger.h"

atomic_int logger_interrupt = 0;

void* logger_func(void* arg) {
    (void)arg;

    while(!logger_interrupt) {
        printf("Logger thread is running...\n");
        usleep(1000000); // Simulate logging activity
    }

    return NULL;
}