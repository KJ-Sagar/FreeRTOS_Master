#include "demo_someip.h"

#include "someip_server.h"
#include "heartbeat_service.h"
#include "sensor_service.h"
#include "engine_service.h"
#include "sd_udp_server.h"
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

    sd_udp_server_start();
    FreeRTOS_printf(("DEMO_SOMEIP: UDP Service Discovery started\r\n"));

    someip_server_start();
    FreeRTOS_printf(("DEMO_SOMEIP: SOME/IP server started\r\n"));   
    /* Start SOME/IP server */
}
