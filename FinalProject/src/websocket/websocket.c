#include "websocket.h"
#include "../utils/utils.h"

atomic_bool is_connected = false;
time_t last_activity = 0;
static time_t last_ping     = 0;

static void subscribe(struct lws *wsi) {
    const char* msg = "{\"op\":\"subscribe\",\"args\":["
        "{\"channel\":\"trades\",\"instId\":\"BTC-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"ADA-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"ETH-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"DOGE-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"XRP-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"SOL-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"LTC-USDT\"},"
        "{\"channel\":\"trades\",\"instId\":\"BNB-USDT\"}"
    "]}";
    size_t len = strlen(msg);
    unsigned char buf[LWS_PRE + len];
    memcpy(&buf[LWS_PRE], msg, len);

    int ret = lws_write(wsi, &buf[LWS_PRE], len, LWS_WRITE_TEXT);
    if (ret < 0) {
        atomic_store(&is_connected, false);
    } else {
        bool* subscribed = lws_wsi_user(wsi);
        *subscribed = true;
    }
}

int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    bool* subscribed = (bool*)user;
    time_t now = time(NULL);

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            atomic_store(&is_connected, true);
            *subscribed = false;
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (!*subscribed) {
                subscribe(wsi);
            } else if (now - last_ping >= 60) {
                unsigned char ping_buf[LWS_PRE];
                int ret = lws_write(wsi, &ping_buf[LWS_PRE], 0, LWS_WRITE_PING);
                if (ret < 0) {
                    atomic_store(&is_connected, false);
                } else {
                    last_ping = now;
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            char *message = malloc(len + 1);
            memcpy(message, in, len);
            message[len] = '\0';
            parse_transaction(message, len, &trade_queue);
            free(message);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            last_activity = now;
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            atomic_store(&is_connected, false);
            break;

        case LWS_CALLBACK_CLOSED:
            atomic_store(&is_connected, false);
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "okx-protocol",
        .callback = websocket_callback,
        .per_session_data_size = sizeof(bool),
        .rx_buffer_size = 4096,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

struct lws_context* websocket_init() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
    info.client_ssl_ca_filepath = "/etc/ssl/certs/ca-certificates.crt";
    info.ka_time = 10;
    info.ka_interval = 5;
    info.ka_probes = 3;

    return lws_create_context(&info);
}

struct lws* websocket_connect(struct lws_context *context) {
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context      = context;
    ccinfo.address      = "ws.okx.com";
    ccinfo.port         = 8443;
    ccinfo.path         = "/ws/v5/public";
    ccinfo.host         = "ws.okx.com";
    ccinfo.origin       = "https://www.okx.com";
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    ccinfo.protocol     = "okx-protocol";
    return lws_client_connect_via_info(&ccinfo);
}