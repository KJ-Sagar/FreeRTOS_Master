#include "demo_someip.h"

#include "someip_server.h"
#include "heartbeat_service.h"
#include "sensor_service.h"
#include "engine_service.h"

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

void demo_someip_start(void)
{
    static BaseType_t started = pdFALSE;

    if (started == pdTRUE)
    {
        FreeRTOS_printf(("DEMO_SOMEIP: already started, skipping\r\n"));
        return;
    }

    started = pdTRUE;
    /* Register SOME/IP services */
    HeartbeatService_Init();
    FreeRTOS_printf(("DEMO_SOMEIP: Heartbeat service registered\r\n"));

    SensorService_Init();
    FreeRTOS_printf(("DEMO_SOMEIP: Sensor service registered\r\n"));

    EngineService_Init();
    FreeRTOS_printf(("DEMO_SOMEIP: Engine service registered\r\n"));

    /* Start SOME/IP server */
    sd_udp_server_start();
    FreeRTOS_printf(("DEMO_SOMEIP: UDP Service Discovery server start requested\r\n"));
    someip_server_start();
    FreeRTOS_printf(("DEMO_SOMEIP: SOME/IP server start requested\r\n"));
}
