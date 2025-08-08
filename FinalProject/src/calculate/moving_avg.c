#include "moving_avg.h"
#include "../utils/utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

void calculate_moving_avg(time_t time_now) {
    struct stat st = {0};
    // Ensure data directory exists
    if (stat("data", &st) == -1) {
        mkdir("data", 0755);
    }

    // Ensure moving average directory exists
    if (stat("data/mavg", &st) == -1) {
        mkdir("data/mavg", 0755);
    }
    
    for(int i = 0; i < 8; i++) {
        pthread_mutex_lock(&symbol_histories[i].mutex);
    
        size_t new_count = 0;
        uint64_t cutoff_time = (uint64_t)(time_now - 900); // 15 minutes
        
        for(size_t j = 0; j < symbol_histories[i].count; j++) {
            if(symbol_histories[i].trades[j].timestamp >= cutoff_time) {
                symbol_histories[i].trades[new_count++] = symbol_histories[i].trades[j];
            }
        }
        symbol_histories[i].count = new_count;

        // Calculate moving average
        double sum_price = 0.0, sum_volume = 0.0;
        for(size_t j = 0; j < symbol_histories[i].count; j++) {
            sum_price  += symbol_histories[i].trades[j].price;
            sum_volume += symbol_histories[i].trades[j].volume;
        }

        double current_ma = (symbol_histories[i].count > 0) ? sum_price / symbol_histories[i].count : 0.0;

        // Update moving average history
        symbol_histories[i].movingAvg_history[symbol_histories[i].movingAvg_index] = current_ma;
        symbol_histories[i].movingAvg_timestamps[symbol_histories[i].movingAvg_index] = time_now;
        symbol_histories[i].movingAvg_index = (symbol_histories[i].movingAvg_index + 1) % 8;
        symbol_histories[i].movingAvg_count = (symbol_histories[i].movingAvg_count < 8) ? symbol_histories[i].movingAvg_count + 1 : 8;

        // Write moving average to log
        char filename[128];
        snprintf(filename, sizeof(filename), "data/mavg/%s.log", symbols[i]);
        
        FILE* file = fopen(filename, "a");
        if(file) {
            int bytes_written = fprintf(file, "%llu,%.8f,%.8f\n", (unsigned long long)time_now, current_ma, sum_volume);
            fflush(file);
            fclose(file);
            printf("DEBUG: Successfully wrote %d bytes to MA file for %s\n", bytes_written, symbols[i]);
            
            // Verify the file was actually written
            struct stat file_stat;
            if (stat(filename, &file_stat) == 0) {
                printf("DEBUG: File %s exists, size: %ld bytes\n", filename, file_stat.st_size);
            }
        } else {
            printf("DEBUG: Failed to open file %s: %s\n", filename, strerror(errno));
        }

        pthread_mutex_unlock(&symbol_histories[i].mutex);
    }
}