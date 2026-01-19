#include "demo_someip.h"
#include "someip_server.h"
#include "FreeRTOS_IP.h"
#include <stdio.h>

void demo_someip_start(void)
{
    FreeRTOS_printf( ( "DEMO_SOMEIP: demo_someip_start() called\r\n" ) );
    someip_server_start();
}

