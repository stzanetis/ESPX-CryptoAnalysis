#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include "websocket/websocket.h"
#include "logger/logger.h"
#include "processor/processor.h"
#include "utils/utils.h"

const char *symbols[] = {"BTC-USDT", "ADA-USDT", "ETH-USDT", "DOGE-USDT", "XRP-USDT", "SOL-USDT", "LTC-USDT", "BNB-USDT"};

volatile sig_atomic_t interrupted = 0;
static struct lws* current_wsi = NULL;
const int max_backoff = 60;
int    backoff      = 2;
time_t last_connect = 0;

void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
    printf("\nShutting down system...\n");
    atomic_store(&logger_interrupt, 1);     // Signal logger to stop
    atomic_store(&processor_interrupt, 1);  // Signal processor to stop
}

int main() {
    printf("Starting Real-Time Cryptocurrency Analysis System...\n");

    // Set up signal handler
    signal(SIGINT, sigint_handler);

    // Initialize Components
    struct lws_context* context = websocket_init();
    queue_init(&trade_queue, 4096); //Queue Size -> 4096

    // Initialize symbol hystory data
    for(int i = 0; i < 8; i++) {
        symbol_histories[i] = (SymbolHistory){
            .trades = NULL,
            .count = 0,
            .capacity = 0,
            .mutex = PTHREAD_MUTEX_INITIALIZER,
            .movingAvg_history = {0},
            .movingAvg_timestamps = {0},
            .movingAvg_index = 0,
        };
    }

    // Create logger and processor threads
    pthread_t logger_thread, processor_thread;
    pthread_create(&logger_thread, NULL, logger_func, &trade_queue);
    pthread_create(&processor_thread, NULL, processor_func, NULL);

    // Monitor connection
    last_activity = time(NULL);
    while(!interrupted) {
        time_t now = time(NULL);

        // Check if we need to reconnect
        if (!current_wsi && !atomic_load(&is_connected)) {
            if (now - last_connect >= backoff) {
                current_wsi = websocket_connect(context);
                last_connect = now;
                if (current_wsi) {
                    printf("Connected to WebSocket server.\n");
                    last_activity = now;  // reset timer on connect attempt
                    backoff = 2;
                } else {
                    printf("Connection failed, retrying...\n");
                    backoff = backoff < max_backoff ? backoff * 2 : max_backoff;
                }
            }
        }

        if(current_wsi) {
            int n = lws_service(context, 50);
            if (n < 0) {
                atomic_store(&is_connected, false);
                current_wsi = NULL;
                continue;
            }

            // Check for inactivity
            if (now - last_activity > 90) {
                atomic_store(&is_connected, false);
                current_wsi = NULL;
            }
        } else {
            usleep(100000); // Sleep for 100ms if not connected
        }
            
    }

    // Clean up for graceful shutdown
    lws_context_destroy(context);
    pthread_join(logger_thread, NULL);
    printf("Logger thread has stopped.\n");
    pthread_join(processor_thread, NULL);
    printf("Processor thread has stopped.\n");

    // Cleanup history data
    for(int i = 0; i < 8; i++) {
        free(symbol_histories[i].trades);
    }

    return 0;
}