#include "demo_selector.h"
#include "app_config.h"
#include "app/demos/demo_someip/heartbeat_service.h"
#include "app/demos/demo_someip/sensor_service.h"
#include "app/demos/demo_someip/engine_service.h"
#include "FreeRTOS_IP.h"

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
