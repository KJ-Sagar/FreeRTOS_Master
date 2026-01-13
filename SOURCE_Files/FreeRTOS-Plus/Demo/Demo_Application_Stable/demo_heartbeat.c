#include "demo_heartbeat.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"
#include "app_config.h"


void vStartHeartbeatDemo( void )
{
    FreeRTOS_printf( ( "HB: Starting Heartbeat demo\n" ) );

    vStartTCPHeartbeatDemo();
}
