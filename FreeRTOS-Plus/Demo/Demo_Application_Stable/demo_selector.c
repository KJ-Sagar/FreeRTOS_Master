#include "demo_selector.h"
#include "app_config.h"
#include "demo_echo.h"
#include "demo_heartbeat.h"

void vSelectAndStartDemo( void )
{
#if APP_DEMO_ECHO
    vStartEchoDemo();
#elif APP_DEMO_HEARTBEAT
    vStartHeartbeatDemo();
#else
    FreeRTOS_printf( ( "No demo selected in app_config.h\r\n" ) );
#endif
}
