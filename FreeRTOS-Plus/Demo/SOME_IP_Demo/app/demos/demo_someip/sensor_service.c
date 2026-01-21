#include "sensor_service.h"
#include "someip_core/someip_core.h"
#include "someip_core/someip_types.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include <string.h>

/* Service + method IDs */
#define SENSOR_SERVICE_ID          0x1001
#define METHOD_GET_TEMPERATURE     0x0001

static BaseType_t sensor_handler(
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

    if (method_id != METHOD_GET_TEMPERATURE)
    {
        *ret = SOMEIP_RET_E_UNKNOWN_METHOD;
        return pdFAIL;
    }

    /* Dummy temperature: 25.0°C represented as int32 */
    int32_t temp = FreeRTOS_htonl(250);
    memcpy(resp, &temp, sizeof(temp));
    *resp_len = sizeof(temp);
    *ret = SOMEIP_RET_OK;

    return pdPASS;
}

void SensorService_Init(void)
{
    someip_register_service(
        SENSOR_SERVICE_ID,
        sensor_handler);
}
