#include "app_config.h"
#include "demo_selector.h"
#include "FreeRTOS_IP.h"

void vStartNetworkRole( void )
{
#if APP_ROLE_CLIENT
    LOGI("NET", "Starting CLIENT role");
    vStartSelectedDemoClient();
#else
    LOGI("NET", "Starting SERVER role");
    vStartSelectedDemoServer();
#endif
}
