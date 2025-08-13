#include "logger.h"
#include "../utils/utils.h"

atomic_int logger_interrupt = 0;

static FILE* log_files[8] = {NULL};

void* logger_func(void* arg) {
    TradeQueue* q = (TradeQueue*)arg;
    TradeData trade;
    
    // Open all log files once
    for(int i = 0; i < 8; i++) {
        char name[128];
        snprintf(name, sizeof(name), "logs/transactions/%s.log", symbols[i]);
        log_files[i] = fopen(name, "a");
    }

    while(!logger_interrupt) {
        queue_pop(q, &trade);
        
        // Find the symbol index and write directly
        for(int i = 0; i < 8; i++) {
            if(strcmp(trade.symbol, symbols[i]) == 0) {
                if(log_files[i]) {
                    fprintf(log_files[i], "[%llu], Price: %.8f, Volume: %.8f\n", (unsigned long long)trade.timestamp, trade.price, trade.volume);
                    fflush(log_files[i]); // Ensure data is written
                }
                break;
            }
        }
    }

    // Close files on exit
    for(int i = 0; i < 8; i++) {
        if(log_files[i]) fclose(log_files[i]);
    }
    
    return NULL;
}