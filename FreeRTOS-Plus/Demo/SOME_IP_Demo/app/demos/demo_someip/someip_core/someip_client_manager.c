#include "someip_server_state.h"
#include "someip_eventgroup.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

/* =========================================================
 * Global Client Table (DEFINITION - not extern)
 * ========================================================= */
someip_client_ctx_t g_someip_clients[SOMEIP_MAX_CLIENTS];

/* =========================================================
 * Initialize Client Table
 * ========================================================= */
void someip_client_table_init(void)
{
    memset(g_someip_clients, 0, sizeof(g_someip_clients));
    
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        g_someip_clients[i].active = pdFALSE;
        g_someip_clients[i].client_state = CLIENT_DISCONNECTED;
        g_someip_clients[i].subscription_count = 0;
        g_someip_clients[i].socket = FREERTOS_INVALID_SOCKET;
        
        /* Create mutex for thread-safe subscription access */
        g_someip_clients[i].mutex = xSemaphoreCreateMutex();
        configASSERT(g_someip_clients[i].mutex != NULL);
    }
}

/* =========================================================
 * Allocate Client Slot
 * ========================================================= */
someip_client_ctx_t* someip_client_allocate(Socket_t socket)
{
    if (socket == FREERTOS_INVALID_SOCKET)
        return NULL;
    
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (!g_someip_clients[i].active)
        {
            /* Initialize client context */
            g_someip_clients[i].socket = socket;
            g_someip_clients[i].active = pdTRUE;
            g_someip_clients[i].client_state = CLIENT_DISCONNECTED;
            g_someip_clients[i].subscription_count = 0;
            g_someip_clients[i].last_activity_tick = xTaskGetTickCount();
            g_someip_clients[i].connect_tick = xTaskGetTickCount();
            g_someip_clients[i].messages_received = 0;
            g_someip_clients[i].notifications_sent = 0;
            
            /* Clear subscription table */
            memset(g_someip_clients[i].subscriptions, 0, 
                   sizeof(g_someip_clients[i].subscriptions));
            
            return &g_someip_clients[i];
        }
    }
    
    return NULL;  /* No free slots */
}

/* =========================================================
 * Find Client by Socket
 * ========================================================= */
someip_client_ctx_t* someip_client_find_by_socket(Socket_t socket)
{
    for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
    {
        if (g_someip_clients[i].active && 
            g_someip_clients[i].socket == socket)
        {
            return &g_someip_clients[i];
        }
    }
    return NULL;
}

/* =========================================================
 * Free Client Slot
 * ========================================================= */
void someip_client_free(someip_client_ctx_t *ctx)
{
    if (ctx == NULL)
        return;
    
    /* Take mutex before modifying */
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE)
    {
        ctx->active = pdFALSE;
        ctx->client_state = CLIENT_DISCONNECTED;
        ctx->subscription_count = 0;
        ctx->socket = FREERTOS_INVALID_SOCKET;
        
        /* Clear all subscriptions */
        memset(ctx->subscriptions, 0, sizeof(ctx->subscriptions));
        
        xSemaphoreGive(ctx->mutex);
    }
}

/* =========================================================
 * Subscribe to Event Group
 * ========================================================= */
BaseType_t someip_client_subscribe(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint32_t ttl_seconds)
{
    if (ctx == NULL || !ctx->active)
        return pdFAIL;
    
    /* Verify event group exists */
    if (someip_eventgroup_find(service_id, eventgroup_id) == NULL)
        return pdFAIL;
    
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return pdFAIL;
    
    /* Check if already subscribed - if so, renew TTL */
    for (uint8_t i = 0; i < ctx->subscription_count; i++)
    {
        if (ctx->subscriptions[i].eventgroup_id == eventgroup_id)
        {
            /* Renew subscription */
            ctx->subscriptions[i].subscription_tick = xTaskGetTickCount();
            ctx->subscriptions[i].expiry_tick = 
                xTaskGetTickCount() + pdMS_TO_TICKS(ttl_seconds * 1000);
            ctx->subscriptions[i].ttl_seconds = ttl_seconds;
            ctx->subscriptions[i].state = EVENTGROUP_SUBSCRIBED;
            
            xSemaphoreGive(ctx->mutex);
            return pdPASS;
        }
    }
    
    /* New subscription - find empty slot */
    if (ctx->subscription_count >= SOMEIP_MAX_SUBSCRIPTIONS_PER_CLIENT)
    {
        xSemaphoreGive(ctx->mutex);
        return pdFAIL;  /* Subscription table full */
    }
    
    /* Add new subscription */
    someip_eventgroup_subscription_t *sub = 
        &ctx->subscriptions[ctx->subscription_count];
    
    sub->eventgroup_id = eventgroup_id;
    sub->state = EVENTGROUP_SUBSCRIBED;
    sub->subscription_tick = xTaskGetTickCount();
    sub->expiry_tick = xTaskGetTickCount() + pdMS_TO_TICKS(ttl_seconds * 1000);
    sub->ttl_seconds = ttl_seconds;
    
    ctx->subscription_count++;
    
    xSemaphoreGive(ctx->mutex);
    return pdPASS;
}

/* =========================================================
 * Unsubscribe from Event Group
 * ========================================================= */
BaseType_t someip_client_unsubscribe(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id)
{
    if (ctx == NULL || !ctx->active)
        return pdFAIL;
    
    (void)service_id;  /* Currently unused */
    
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return pdFAIL;
    
    /* Find and remove subscription */
    for (uint8_t i = 0; i < ctx->subscription_count; i++)
    {
        if (ctx->subscriptions[i].eventgroup_id == eventgroup_id)
        {
            /* Shift remaining subscriptions down */
            for (uint8_t j = i; j < ctx->subscription_count - 1; j++)
            {
                ctx->subscriptions[j] = ctx->subscriptions[j + 1];
            }
            
            ctx->subscription_count--;
            
            /* Clear last entry */
            memset(&ctx->subscriptions[ctx->subscription_count], 0,
                   sizeof(someip_eventgroup_subscription_t));
            
            xSemaphoreGive(ctx->mutex);
            return pdPASS;
        }
    }
    
    xSemaphoreGive(ctx->mutex);
    return pdFAIL;  /* Not found */
}

/* =========================================================
 * Check if Subscribed
 * ========================================================= */
BaseType_t someip_client_is_subscribed(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id)
{
    if (ctx == NULL || !ctx->active)
        return pdFALSE;
    
    (void)service_id;
    
    BaseType_t result = pdFALSE;
    
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        TickType_t now = xTaskGetTickCount();
        
        for (uint8_t i = 0; i < ctx->subscription_count; i++)
        {
            if (ctx->subscriptions[i].eventgroup_id == eventgroup_id &&
                ctx->subscriptions[i].state == EVENTGROUP_SUBSCRIBED &&
                now < ctx->subscriptions[i].expiry_tick)
            {
                result = pdTRUE;
                break;
            }
        }
        
        xSemaphoreGive(ctx->mutex);
    }
    
    return result;
}

/* =========================================================
 * Renew Subscription
 * ========================================================= */
BaseType_t someip_client_renew_subscription(
    someip_client_ctx_t *ctx,
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint32_t ttl_seconds)
{
    /* Renew is same as subscribe - it will update TTL if exists */
    return someip_client_subscribe(ctx, service_id, eventgroup_id, ttl_seconds);
}

/* =========================================================
 * Check TTL Expiration
 * ========================================================= */
uint8_t someip_client_check_ttl_expiration(someip_client_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->active)
        return 0;
    
    uint8_t expired_count = 0;
    
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        TickType_t now = xTaskGetTickCount();
        
        for (uint8_t i = 0; i < ctx->subscription_count; i++)
        {
            if (ctx->subscriptions[i].state == EVENTGROUP_SUBSCRIBED &&
                now >= ctx->subscriptions[i].expiry_tick)
            {
                ctx->subscriptions[i].state = EVENTGROUP_EXPIRED;
                expired_count++;
            }
        }
        
        xSemaphoreGive(ctx->mutex);
    }
    
    return expired_count;
}