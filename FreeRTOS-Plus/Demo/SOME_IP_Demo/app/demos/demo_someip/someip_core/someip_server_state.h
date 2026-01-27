#ifndef SOMEIP_SERVER_STATE_H
#define SOMEIP_SERVER_STATE_H

#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"
#include "task.h"
#include "semphr.h"
#include "someip_eventgroup.h"

/* =========================================================
 * Configuration
 * ========================================================= */
#define SOMEIP_MAX_CLIENTS  4
#define SOMEIP_MAX_SUBSCRIPTIONS_PER_CLIENT  8

/* =========================================================
 * Client State Machine
 * ========================================================= */
typedef enum {
    CLIENT_DISCONNECTED = 0,
    CLIENT_CONNECTED,
    CLIENT_ACTIVE
} someip_client_state_t;

/* =========================================================
 * Client Context (Enhanced with Event Groups)
 * ========================================================= */
typedef struct {
    /* Connection */
    Socket_t socket;
    BaseType_t active;
    someip_client_state_t client_state;
    
    /* Subscriptions (Event Group based) */
    someip_eventgroup_subscription_t subscriptions[SOMEIP_MAX_SUBSCRIPTIONS_PER_CLIENT];
    uint8_t subscription_count;
    
    /* Lifecycle */
    TickType_t last_activity_tick;
    TickType_t connect_tick;
    TaskHandle_t rx_task;
    
    /* Thread Safety */
    SemaphoreHandle_t mutex;  /* Protects subscription table */
    
    /* Statistics (Optional) */
    uint32_t messages_received;
    uint32_t notifications_sent;
} someip_client_ctx_t;

/* =========================================================
 * Global Client Table
 * ========================================================= */
extern someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];

/* =========================================================
 * Client Management Functions
 * ========================================================= */

/**
 * Initialize the client table (call once at startup)
 */
void someip_client_table_init(void);

/**
 * Allocate a client slot for a new connection
 * Returns pointer to allocated context or NULL if table is full
 */
someip_client_ctx_t* someip_client_allocate(Socket_t socket);

/**
 * Find client context by socket
 * Returns pointer to context or NULL if not found
 */
someip_client_ctx_t* someip_client_find_by_socket(Socket_t socket);

/**
 * Free a client slot (call on disconnect)
 */
void someip_client_free(someip_client_ctx_t *ctx);

/* =========================================================
 * Subscription Management Functions
 * ========================================================= */

/**
 * Subscribe client to an event group
 * Returns pdPASS on success, pdFAIL if subscription table is full
 */
BaseType_t someip_client_subscribe(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint32_t ttl_seconds
);

/**
 * Unsubscribe client from an event group
 * Returns pdPASS if unsubscribed, pdFAIL if not found
 */
BaseType_t someip_client_unsubscribe(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id
);

/**
 * Check if client is subscribed to a specific event group
 * Returns pdTRUE if subscribed and not expired, pdFALSE otherwise
 */
BaseType_t someip_client_is_subscribed(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id
);

/**
 * Renew subscription (extend TTL)
 * Returns pdPASS if renewed, pdFAIL if subscription doesn't exist
 */
BaseType_t someip_client_renew_subscription(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint32_t ttl_seconds
);

/**
 * Check and expire TTL for all subscriptions of a client
 * Returns number of expired subscriptions
 */
uint8_t someip_client_check_ttl_expiration(someip_client_ctx_t *ctx);

#endif /* SOMEIP_SERVER_STATE_H */