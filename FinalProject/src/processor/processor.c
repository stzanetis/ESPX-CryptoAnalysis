#include "processor.h"
#include "../calculate/moving_avg.h"
#include "../calculate/correlation.h"

atomic_int processor_interrupt = 0;

void* processor_func(void* arg __attribute__((unused))) {

    // Initialize timer for periodic data processing
    struct itimerspec its;
    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);

    int timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    its.it_value.tv_sec = (time_now.tv_sec / 60 + 1) * 60;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 60;
    its.it_interval.tv_nsec = 0;
    timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL);

    while(!processor_interrupt) {
        // Wait for the timer to expire
        uint64_t exp;
        read(timer_fd, &exp, sizeof(exp));

        // Process data
        time_t current_time = time(NULL);
        calculate_moving_avg(current_time);
        printf("DEBUG: Processed moving averages at %s", ctime(&current_time));
        calculate_correlation(current_time);
    }

    return NULL;
}