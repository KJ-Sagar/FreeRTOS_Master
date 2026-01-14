#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

#include "app_config.h"
#include "app_main.h"
#include "demo_echo.h"
#include "tcp_heartbeat_demo.h"

void vApplicationStart( void )
{
#if DEMO_HEARTBEAT
    vStartTCPHeartbeatDemo();
#elif DEMO_ECHO
    vStartEchoDemo();
#else
    FreeRTOS_printf( ( "APP: No demo selected\r\n" ) );
#endif
}
/*-----------------------------------------------------------*/