#include "FreeRTOS.h"
#include "task.h"
#include "someip_server_state.h"
#include "FreeRTOS_IP.h"
#include "app/demos/demo_someip/someip_core/someip_server_state.h"
/* =========================================================
 * Configuration
 * ========================================================= */
#define TTL_CHECK_PERIOD_MS  1000  /* Check every 1 second */

/* =========================================================
 * TTL Manager Task
 * 
 * Periodically checks all client subscriptions for TTL
 * expiration and marks them as EXPIRED.
 * ========================================================= */
static void someip_ttl_manager_task(void *arg)
{
    (void)arg;
    
    FreeRTOS_printf(("SOME/IP: TTL Manager started\r\n"));
    
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(TTL_CHECK_PERIOD_MS));
        
        /* Check all active clients */
        for (int i = 0; i < SOMEIP_MAX_CLIENTS; i++)
        {
            if (!g_someip_clients[i].active)
                continue;
            
            uint8_t expired = someip_client_check_ttl_expiration(&g_someip_clients[i]);
            
            if (expired > 0)
            {
                FreeRTOS_printf((
                    "SOME/IP: Client[%d] - %u subscription(s) expired\r\n",
                    i, expired
                ));
            }
        }
    }
}

/* =========================================================
 * Start TTL Manager
 * ========================================================= */
void someip_ttl_manager_start(void)
{
    xTaskCreate(
        someip_ttl_manager_task,
        "SOMEIP_TTL",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
}