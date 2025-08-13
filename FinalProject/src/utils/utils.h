#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <cjson/cJSON.h>

typedef struct {
    char symbol[16];
    double price;
    double volume;
    uint64_t timestamp;
} TradeData;

typedef struct TradeQueue {
    TradeData* data;
    size_t size;
    size_t head;
    size_t tail;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} TradeQueue;

typedef struct {
    TradeData* trades;
    size_t count;
    size_t capacity;
    pthread_mutex_t mutex;

    // Last 8 moving average values
    double movingAvg_history[8];
    time_t movingAvg_timestamps[8];   
    int movingAvg_index;
    int movingAvg_count;
} SymbolHistory;

extern const char* symbols[8];
extern SymbolHistory symbol_histories[8];
extern TradeQueue trade_queue;
extern atomic_int logger_interrupt;

void queue_init(TradeQueue* q, size_t size);
void queue_push(TradeQueue* q, TradeData* trade);
void queue_pop (TradeQueue* q, TradeData* trade);
void parse_transaction(const char* json_str, size_t len, TradeQueue* queue);
void log_time(struct timespec* start, struct timespec* end);