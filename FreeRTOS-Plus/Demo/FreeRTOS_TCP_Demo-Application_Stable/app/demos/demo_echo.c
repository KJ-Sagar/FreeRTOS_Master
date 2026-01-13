#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP.h"

#include "app_config.h"
#include "demo_echo.h"
#include "TCPEchoClient_SingleTasks.h"

void vStartEchoDemo( void )
{
#if APP_ROLE_CLIENT
    FreeRTOS_printf( ( "ECHO: Starting TCP Echo CLIENT\r\n" ) );

    vStartTCPEchoClientTasks_SingleTasks(
        configMINIMAL_STACK_SIZE * 2,
        tskIDLE_PRIORITY + 1
    );

#elif APP_ROLE_SERVER
    FreeRTOS_printf( ( "ECHO: Starting TCP Echo SERVER\r\n" ) );
    vStartEchoServer();
#endif
}
/*-----------------------------------------------------------*/