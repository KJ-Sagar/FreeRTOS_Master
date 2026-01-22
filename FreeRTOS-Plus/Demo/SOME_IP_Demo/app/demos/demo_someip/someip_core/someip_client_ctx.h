#pragma once

#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"
#include "task.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct someip_client_ctx {
    Socket_t socket;

    uint16_t client_id;
    uint16_t next_session_id;

    bool heartbeat_subscribed;

    TickType_t last_activity;

    struct someip_client_ctx *next;  // linked list
} someip_client_ctx_t;

/* lifecycle */
someip_client_ctx_t *someip_client_create(Socket_t sock);
void someip_client_destroy(someip_client_ctx_t *ctx);

/* registry */
void someip_client_register(someip_client_ctx_t *ctx);
void someip_client_unregister(someip_client_ctx_t *ctx);

someip_client_ctx_t *someip_client_get_head(void);
