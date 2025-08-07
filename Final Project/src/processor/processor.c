#include "processor.h"

atomic_int processor_interrupt = 0;

void* processor_func(void* arg) {
    (void)arg;

    while(!processor_interrupt) {
        printf("Processor thread is running...\n");
        usleep(1000000); // Simulate logging activity
    }

    return NULL;
}