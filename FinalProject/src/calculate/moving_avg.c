#include "moving_avg.h"
#include "../utils/utils.h"

void calculate_moving_avg(time_t time_now) {
    struct stat st = {0};
    if (stat("data", &st) == -1) mkdir("data", 0755);
    if (stat("data/mavg", &st) == -1) mkdir("data/mavg", 0755);
    
    for(int i = 0; i < 8; i++) {
        pthread_mutex_lock(&symbol_histories[i].mutex);
        
        // Purge old trades
        size_t new_count = 0;
        for(size_t j = 0; j < symbol_histories[i].count; j++) {
            if(symbol_histories[i].trades[j].timestamp >= (uint64_t)time_now - 900) {
                symbol_histories[i].trades[new_count++] = symbol_histories[i].trades[j];
            }
        }
        symbol_histories[i].count = new_count;

        // Calculate moving average
        double sum_price = 0.0, sum_volume = 0.0;
        for(size_t j=0; j<symbol_histories[i].count; j++) {
            sum_price  += symbol_histories[i].trades[j].price;
            sum_volume += symbol_histories[i].trades[j].volume;
        }
        
        double current_ma = (symbol_histories[i].count > 0) ? 
            sum_price / symbol_histories[i].count : 0.0;
        
        // Store in circular buffer
        symbol_histories[i].movingAvg_history[symbol_histories[i].movingAvg_index] = current_ma;
        symbol_histories[i].movingAvg_timestamps[symbol_histories[i].movingAvg_index] = time_now;
        symbol_histories[i].movingAvg_index = (symbol_histories[i].movingAvg_index + 1) % 8;
        symbol_histories[i].movingAvg_count = (symbol_histories[i].movingAvg_count < 8) ? symbol_histories[i].movingAvg_count + 1 : 8;
        
        // Write to file
        char filename[128];
        snprintf(filename, sizeof(filename), "data/mavg/%s.log", symbols[i]);
        FILE* file = fopen(filename, "a");
        if(file) {
            fprintf(file, "%llu,%.8f\n", (unsigned long long)time_now, current_ma);
            fflush(file);
            fclose(file);
        }
        
        pthread_mutex_unlock(&symbol_histories[i].mutex);
    }
}