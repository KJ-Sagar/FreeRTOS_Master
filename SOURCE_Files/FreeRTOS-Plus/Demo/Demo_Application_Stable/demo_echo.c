#include "FreeRTOS.h"
#include "app_config.h"
#include "demo_echo_server.h"
#include "TCPEchoClient_SingleTasks.h"

void vStartEchoDemo( void )
{
#if APP_ROLE_CLIENT
    FreeRTOS_printf( ( "Starting TCP Echo CLIENT\r\n" ) );

    vStartTCPEchoClientTasks_SingleTasks(
        configMINIMAL_STACK_SIZE * 2,
        tskIDLE_PRIORITY + 1
    );

#elif APP_ROLE_SERVER
    FreeRTOS_printf( ( "Starting TCP Echo SERVER\r\n" ) );
    vStartEchoServer();
#endif
}
