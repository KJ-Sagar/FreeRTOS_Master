#include "demo_someip.h"

#include "someip_server.h"
#include "sd_udp_server.h"
#include "heartbeat_service.h"
#include "sensor_service.h"
#include "engine_service.h"

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

void demo_someip_start(void)
{
    FreeRTOS_printf(("DEMO_SOMEIP: Heartbeat service registered\r\n"));
    HeartbeatService_Init();

    FreeRTOS_printf(("DEMO_SOMEIP: Sensor service registered\r\n"));
    SensorService_Init();

    FreeRTOS_printf(("DEMO_SOMEIP: Engine service registered\r\n"));
    EngineService_Init();

    FreeRTOS_printf(("DEMO_SOMEIP: Service Discovery offer started\r\n"));
    sd_udp_server_start();

    FreeRTOS_printf(("DEMO_SOMEIP: SOME/IP server started\r\n"));
    someip_server_start();
}
