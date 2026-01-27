#include "heartbeat_service.h"
#include "someip_core/someip_core.h"
#include "someip_core/someip_types.h"
#include "app/demos/demo_someip/someip_core/someip_eventgroup.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include <string.h>

#define SOMEIP_SERVICE_ID_HEARTBEAT  0x1234U
#define SOMEIP_METHOD_ID_HEARTBEAT   0x0001U
#define SOMEIP_EVENTGROUP_STATUS     0x0001U

static BaseType_t heartbeat_handler(
    uint16_t service_id,
    uint16_t method_id,
    const uint8_t *req,
    uint32_t req_len,
    uint8_t *resp,
    uint32_t *resp_len,
    someip_return_code_t *ret)
{
    (void)service_id;
    (void)req;
    (void)req_len;

    if (resp == NULL || resp_len == NULL || ret == NULL)
        return pdFAIL;

    if (method_id != SOMEIP_METHOD_ID_HEARTBEAT)
    {
        *ret = SOMEIP_RET_E_UNKNOWN_METHOD;
        return pdFAIL;
    }

    uint32_t alive = FreeRTOS_htonl(1U);
    memcpy(resp, &alive, sizeof(alive));
    *resp_len = sizeof(alive);
    *ret = SOMEIP_RET_OK;

    return pdPASS;
}

void HeartbeatService_Init(void)
{
    /* Register service handler (Phase 3 - unchanged) */
    someip_register_service(
        SOMEIP_SERVICE_ID_HEARTBEAT,
        heartbeat_handler
    );

    /* NEW Phase 4: Register event group */
    static const someip_eventgroup_def_t heartbeat_eventgroup = {
        .service_id = SOMEIP_SERVICE_ID_HEARTBEAT,
        .eventgroup_id = SOMEIP_EVENTGROUP_STATUS,
        .events = {
            {
                .event_id = SOMEIP_METHOD_ID_HEARTBEAT,
                .eventgroup_id = SOMEIP_EVENTGROUP_STATUS,
                .update_period_ms = 2000  /* 2 second period */
            }
        },
        .event_count = 1,
        .default_ttl_seconds = 5  /* 5 second TTL */
    };

    BaseType_t result = someip_eventgroup_register(&heartbeat_eventgroup);
    
    if (result == pdPASS)
    {
        FreeRTOS_printf((
            "Heartbeat: Event group 0x%04x registered\r\n",
            SOMEIP_EVENTGROUP_STATUS
        ));
    }
    else
    {
        FreeRTOS_printf((
            "Heartbeat: Failed to register event group\r\n"
        ));
    }
}