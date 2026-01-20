#include "demo_someip.h"

#include "someip_server.h"
#include "heartbeat_service.h"

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

void demo_someip_start(void)
{
    FreeRTOS_printf(("DEMO_SOMEIP: demo_someip_start() called\r\n"));

    /* Register SOME/IP services */
    HeartbeatService_Init();
    FreeRTOS_printf(("DEMO_SOMEIP: Heartbeat service registered\r\n"));

    /* Start SOME/IP server */
    someip_server_start();
    FreeRTOS_printf(("DEMO_SOMEIP: SOME/IP server start requested\r\n"));
}
