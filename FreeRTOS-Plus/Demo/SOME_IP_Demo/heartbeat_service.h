#include "heartbeat_service.h"
#include "someip_core.h"
#include "someip_protocol.h"
#include "FreeRTOS.h"
#include <string.h>

#define SOMEIP_SERVICE_ID_HEARTBEAT  0x1234
#define SOMEIP_METHOD_ID_HEARTBEAT   0x0001

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

    if (method_id != SOMEIP_METHOD_ID_HEARTBEAT)
    {
        *ret = SOMEIP_RET_E_UNKNOWN_METHOD;
        return pdFAIL;
    }

    uint32_t alive = FreeRTOS_htonl(1);
    memcpy(resp, &alive, sizeof(alive));
    *resp_len = sizeof(alive);
    *ret = SOMEIP_RET_OK;

    return pdPASS;
}

void HeartbeatService_Init(void)
{
    someip_register_service(
        SOMEIP_SERVICE_ID_HEARTBEAT,
        heartbeat_handler);
}
