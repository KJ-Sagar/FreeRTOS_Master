#include "someip_core.h"

/*
 * Service registry entry
 */
typedef struct
{
    uint16_t service_id;
    someip_service_handler_t handler;
} someip_service_entry_t;

/*
 * Static service registry (deterministic, no dynamic allocation)
 */
static someip_service_entry_t service_table[SOMEIP_MAX_SERVICES] = {0};

BaseType_t someip_register_service(
    uint16_t service_id,
    someip_service_handler_t handler)
{
    if (handler == NULL)
        return pdFAIL;

    /* Prevent duplicate registration of same service ID */
    for (int i = 0; i < SOMEIP_MAX_SERVICES; i++)
    {
        if (service_table[i].handler != NULL &&
            service_table[i].service_id == service_id)
        {
            return pdFAIL;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < SOMEIP_MAX_SERVICES; i++)
    {
        if (service_table[i].handler == NULL)
        {
            service_table[i].service_id = service_id;
            service_table[i].handler = handler;
            return pdPASS;
        }
    }

    return pdFAIL;
}

someip_service_handler_t someip_find_service(uint16_t service_id)
{
    for (int i = 0; i < SOMEIP_MAX_SERVICES; i++)
    {
        if (service_table[i].handler != NULL &&
            service_table[i].service_id == service_id)
        {
            return service_table[i].handler;
        }
    }
    return NULL;
}
