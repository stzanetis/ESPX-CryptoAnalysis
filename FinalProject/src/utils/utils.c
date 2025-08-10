#include "utils.h"
#include <errno.h>
#include <time.h>

TradeQueue trade_queue;
SymbolHistory symbol_histories[8] = {0};

void queue_init(TradeQueue* q, size_t size) {
    q->data = malloc(size*sizeof(TradeData));
    q->size = size;
    q->head = q->tail = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void queue_push(TradeQueue* q, TradeData* trade) {
    pthread_mutex_lock(&q->lock);
    q->data[q->tail] = *trade;
    q->tail = (q->tail+1) % q->size;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

void queue_pop(TradeQueue* q, TradeData* trade) {
    pthread_mutex_lock(&q->lock);
    
    while(q->head == q->tail) {
        // Check if logger should stop before waiting
        if (atomic_load(&logger_interrupt)) {
            pthread_mutex_unlock(&q->lock);
            return; // Exit gracefully
        }
        
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // Wait up to a second
        
        int result = pthread_cond_timedwait(&q->not_empty, &q->lock, &timeout);
        if (result == ETIMEDOUT) {
            continue;
        }
    }

    *trade = q->data[q->head];
    q->head = (q->head+1) % q->size;
    pthread_mutex_unlock(&q->lock);
}

void parse_transaction(const char* json_str, size_t len, TradeQueue* queue) {
    if (!json_str) {
        return;
    }

    cJSON *json = cJSON_ParseWithLength(json_str, len);;
    if (json == NULL) {
        return; // Skip non-JSON messages
    }
    
    // Get the data array
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(json);
        return;
    }

    // Iterate through trades in the data array
    cJSON *trade = NULL;
    cJSON_ArrayForEach(trade, data) {
        TradeData tdata;

        // Extract trade data
        cJSON *instId = cJSON_GetObjectItem(trade, "instId");
        cJSON *px = cJSON_GetObjectItem(trade, "px");
        cJSON *sz = cJSON_GetObjectItem(trade, "sz");
        cJSON *ts = cJSON_GetObjectItem(trade, "ts");
        
        // Verify all required fields are strings
        if (cJSON_IsString(instId) && cJSON_IsString(px) && cJSON_IsString(sz) && cJSON_IsString(ts)) {
            
            // Copy symbol with bounds checking
            strncpy(tdata.symbol, instId->valuestring, sizeof(tdata.symbol) - 1);
            tdata.symbol[sizeof(tdata.symbol) - 1] = '\0';
            
            // Convert string values to appropriate types
            tdata.price = atof(px->valuestring);
            tdata.volume = atof(sz->valuestring);
            tdata.timestamp = strtoull(ts->valuestring, NULL, 10) / 1000;
            
            // Add to queue
            queue_push(queue, &tdata);

            // Add to symbol histories
            for(int i = 0; i < 8; i++) {
                if(strcmp(tdata.symbol, symbols[i]) == 0) {
                    pthread_mutex_lock(&symbol_histories[i].mutex);
                    
                    // Clean old trades first
                    time_t cutoff = tdata.timestamp - 900;
                    size_t write_pos = 0;
                    
                    // Compact array by removing old trades
                    for(size_t read_pos = 0; read_pos < symbol_histories[i].count; read_pos++) {
                        if(symbol_histories[i].trades[read_pos].timestamp >= (uint64_t)cutoff) {
                            if(write_pos != read_pos) {
                                symbol_histories[i].trades[write_pos] = symbol_histories[i].trades[read_pos];
                            }
                            write_pos++;
                        }
                    }
                    symbol_histories[i].count = write_pos;
                    
                    // Resize array if needed
                    if(symbol_histories[i].count >= symbol_histories[i].capacity) {
                        symbol_histories[i].capacity = symbol_histories[i].capacity ? symbol_histories[i].capacity * 2 : 128;
                        symbol_histories[i].trades = realloc(symbol_histories[i].trades, symbol_histories[i].capacity * sizeof(TradeData));
                    }
                    
                    // Add trade to history
                    symbol_histories[i].trades[symbol_histories[i].count++] = tdata;
                    pthread_mutex_unlock(&symbol_histories[i].mutex);
                    break;
                }
            }
        }
    }
    
    // Clean up
    cJSON_Delete(json);
}