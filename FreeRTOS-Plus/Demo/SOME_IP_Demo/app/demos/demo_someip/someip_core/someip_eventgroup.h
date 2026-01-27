#ifndef SOMEIP_EVENTGROUP_H
#define SOMEIP_EVENTGROUP_H

#include "FreeRTOS.h"
#include <stdint.h>

/* =========================================================
 * Event Group Configuration
 * ========================================================= */
#define SOMEIP_MAX_EVENTGROUPS_PER_SERVICE  4
#define SOMEIP_MAX_EVENTS_PER_EVENTGROUP    8

/* =========================================================
 * Event Group Subscription State
 * ========================================================= */
typedef enum {
    EVENTGROUP_NOT_SUBSCRIBED = 0,
    EVENTGROUP_SUBSCRIBED,
    EVENTGROUP_EXPIRED
} someip_eventgroup_state_t;

/* =========================================================
 * Event Group Subscription Entry
 * ========================================================= */
typedef struct {
    uint16_t eventgroup_id;
    someip_eventgroup_state_t state;
    TickType_t subscription_tick;  /* When subscription was made */
    TickType_t expiry_tick;        /* When subscription expires */
    uint32_t ttl_seconds;          /* Configured TTL */
} someip_eventgroup_subscription_t;

/* =========================================================
 * Event Definition
 * ========================================================= */
typedef struct {
    uint16_t event_id;             /* Event/Method ID */
    uint16_t eventgroup_id;        /* Which event group this belongs to */
    uint32_t update_period_ms;     /* How often to send (0 = on-change) */
} someip_event_def_t;

/* =========================================================
 * Event Group Definition
 * ========================================================= */
typedef struct {
    uint16_t service_id;
    uint16_t eventgroup_id;
    someip_event_def_t events[SOMEIP_MAX_EVENTS_PER_EVENTGROUP];
    uint8_t event_count;
    uint32_t default_ttl_seconds;  /* Default TTL for this event group */
} someip_eventgroup_def_t;

/* =========================================================
 * Event Group Registry Functions
 * ========================================================= */

/**
 * Register an event group definition
 * Returns pdPASS on success, pdFAIL if registry is full
 */
BaseType_t someip_eventgroup_register(
    const someip_eventgroup_def_t *eventgroup_def
);

/**
 * Find event group definition by service and eventgroup ID
 * Returns pointer to definition or NULL if not found
 */
const someip_eventgroup_def_t* someip_eventgroup_find(
    uint16_t service_id,
    uint16_t eventgroup_id
);

/**
 * Check if an event belongs to a specific event group
 * Returns pdTRUE if event_id belongs to eventgroup_id, pdFALSE otherwise
 */
BaseType_t someip_eventgroup_contains_event(
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint16_t event_id
);

/**
 * Get default TTL for an event group
 * Returns TTL in seconds, or 0 if event group not found
 */
uint32_t someip_eventgroup_get_default_ttl(
    uint16_t service_id,
    uint16_t eventgroup_id
);

#endif /* SOMEIP_EVENTGROUP_H */