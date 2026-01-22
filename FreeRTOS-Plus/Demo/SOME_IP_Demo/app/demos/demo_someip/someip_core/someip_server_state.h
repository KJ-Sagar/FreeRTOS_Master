#pragma once

#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"

#define SOMEIP_MAX_CLIENTS  4

typedef struct
{
    Socket_t socket;
    BaseType_t active;
    BaseType_t heartbeat_subscribed;
    TickType_t last_activity_tick;
    TaskHandle_t rx_task;
    TaskHandle_t notify_task;
} someip_client_ctx_t;

/* Global client table */
extern someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];
