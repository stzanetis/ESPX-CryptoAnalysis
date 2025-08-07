#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include "websocket/websocket.h"
#include "logger/logger.h"
#include "processor/processor.h"

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
}

int main() {
    printf("Starting Real-Time Cryptocurrency Analysis System...\n");

    // Set up signal handler
    signal(SIGINT, sigint_handler);

    // Initialize WebSocket connection
    struct lws_context* context = websocket_init();

    // Create logger and processor threads
    pthread_t logger_thread, processor_thread;
    pthread_create(&logger_thread, NULL, logger_func, NULL);
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
    atomic_store(&logger_interrupt, 1);     // Signal logger to stop
    pthread_join(logger_thread, NULL);
    printf("Logger thread has stopped.\n");
    atomic_store(&processor_interrupt, 1);  // Signal processor to stop
    pthread_join(processor_thread, NULL);
    printf("Processor thread has stopped.\n");

    return 0;
}