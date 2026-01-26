#ifndef SOMEIP_SERVER_STATE_H
#define SOMEIP_SERVER_STATE_H

#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"
#include "task.h"

/* =========================================================
 * Forward declarations (enums BEFORE struct)
 * ========================================================= */
typedef enum {
    CLIENT_DISCONNECTED = 0,
    CLIENT_CONNECTED,
    CLIENT_ACTIVE
} someip_client_state_t;

typedef enum {
    EVENT_NOT_SUBSCRIBED = 0,
    EVENT_SUBSCRIBED
} someip_event_state_t;

/* =========================================================
 * Client context (now enums are defined)
 * ========================================================= */
#define SOMEIP_MAX_CLIENTS  4

typedef struct {
    Socket_t socket;
    BaseType_t active;
    BaseType_t heartbeat_subscribed;  // Legacy - can remove later
    TickType_t last_activity_tick;
    TaskHandle_t rx_task;
    TaskHandle_t notify_task;         // Not used yet
    someip_client_state_t client_state;
    someip_event_state_t event_state;
} someip_client_ctx_t;

/* =========================================================
 * Global client table
 * ========================================================= */
extern someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];

#endif /* SOMEIP_SERVER_STATE_H */