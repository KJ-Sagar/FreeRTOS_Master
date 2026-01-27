#include "someip_eventgroup.h"
#include <string.h>

/* =========================================================
 * Event Group Registry (Static Allocation)
 * ========================================================= */
#define SOMEIP_MAX_EVENTGROUP_DEFS  16

static someip_eventgroup_def_t eventgroup_registry[SOMEIP_MAX_EVENTGROUP_DEFS];
static uint8_t eventgroup_count = 0;

/* =========================================================
 * Register Event Group Definition
 * ========================================================= */
BaseType_t someip_eventgroup_register(
    const someip_eventgroup_def_t *eventgroup_def)
{
    if (eventgroup_def == NULL)
        return pdFAIL;

    if (eventgroup_count >= SOMEIP_MAX_EVENTGROUP_DEFS)
        return pdFAIL;

    /* Check for duplicate registration */
    for (uint8_t i = 0; i < eventgroup_count; i++)
    {
        if (eventgroup_registry[i].service_id == eventgroup_def->service_id &&
            eventgroup_registry[i].eventgroup_id == eventgroup_def->eventgroup_id)
        {
            return pdFAIL;  /* Already registered */
        }
    }

    /* Copy definition to registry */
    memcpy(&eventgroup_registry[eventgroup_count], 
           eventgroup_def, 
           sizeof(someip_eventgroup_def_t));
    
    eventgroup_count++;
    return pdPASS;
}

/* =========================================================
 * Find Event Group Definition
 * ========================================================= */
const someip_eventgroup_def_t* someip_eventgroup_find(
    uint16_t service_id,
    uint16_t eventgroup_id)
{
    for (uint8_t i = 0; i < eventgroup_count; i++)
    {
        if (eventgroup_registry[i].service_id == service_id &&
            eventgroup_registry[i].eventgroup_id == eventgroup_id)
        {
            return &eventgroup_registry[i];
        }
    }
    return NULL;
}

/* =========================================================
 * Check if Event Belongs to Event Group
 * ========================================================= */
BaseType_t someip_eventgroup_contains_event(
    uint16_t service_id,
    uint16_t eventgroup_id,
    uint16_t event_id)
{
    const someip_eventgroup_def_t *eg = someip_eventgroup_find(
        service_id, 
        eventgroup_id
    );

    if (eg == NULL)
        return pdFALSE;

    for (uint8_t i = 0; i < eg->event_count; i++)
    {
        if (eg->events[i].event_id == event_id)
            return pdTRUE;
    }

    return pdFALSE;
}

/* =========================================================
 * Get Default TTL for Event Group
 * ========================================================= */
uint32_t someip_eventgroup_get_default_ttl(
    uint16_t service_id,
    uint16_t eventgroup_id)
{
    const someip_eventgroup_def_t *eg = someip_eventgroup_find(
        service_id, 
        eventgroup_id
    );

    return (eg != NULL) ? eg->default_ttl_seconds : 0;
}