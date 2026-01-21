#include "engine_service.h"
#include "someip_core/someip_core.h"
#include "someip_core/someip_types.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include <string.h>

#define ENGINE_SERVICE_ID     0x1002
#define METHOD_GET_RPM        0x0010

static BaseType_t engine_handler(
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

    if (method_id != METHOD_GET_RPM)
    {
        *ret = SOMEIP_RET_E_UNKNOWN_METHOD;
        return pdFAIL;
    }

    uint16_t rpm = FreeRTOS_htons(1800);
    memcpy(resp, &rpm, sizeof(rpm));
    *resp_len = sizeof(rpm);
    *ret = SOMEIP_RET_OK;

    return pdPASS;
}

void EngineService_Init(void)
{
    someip_register_service(
        ENGINE_SERVICE_ID,
        engine_handler);
}
