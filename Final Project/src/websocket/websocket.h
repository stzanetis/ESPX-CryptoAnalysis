#include <libwebsockets.h>
#include <stdatomic.h>
#include <stdbool.h>

extern atomic_bool is_connected;
extern time_t last_activity;

int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
struct lws_context* websocket_init();
struct lws* websocket_connect(struct lws_context *context);
int monitor_connection(struct lws_context* context);