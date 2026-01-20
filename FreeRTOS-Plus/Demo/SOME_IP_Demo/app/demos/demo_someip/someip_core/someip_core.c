#include "someip_core.h"

typedef struct
{
    uint16_t service_id;
    someip_service_handler_t handler;
} someip_service_entry_t;

static someip_service_entry_t service_table[SOMEIP_MAX_SERVICES];

BaseType_t someip_register_service(
    uint16_t service_id,
    someip_service_handler_t handler)
{
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
        if (service_table[i].handler &&
            service_table[i].service_id == service_id)
        {
            return service_table[i].handler;
        }
    }
    return NULL;
}
