#include "FreeRTOS.h"
#include "task.h"
#include "demo_selector.h"

/* This function is called from the network-up hook */
void vApplicationStart( void )
{
    FreeRTOS_printf( ( "APP: Application start\n" ) );

    /* Select and start demo based on config */
    vDemoSelectorStart();
}
