#ifndef SOMEIP_SERVER_H
#define SOMEIP_SERVER_H

#include "FreeRTOS.h"

/*
 * SOME/IP server configuration
 * Transport-specific details (sockets) must remain outside
 * the SOME/IP runtime logic.
 */

#define SOMEIP_SERVER_PORT        30509U
#define SOMEIP_RX_BUFFER_SIZE     256U
#define SOMEIP_TX_BUFFER_SIZE     256U

/*
 * Start the SOME/IP server runtime.
 * This creates the SOME/IP server task.
 */
void someip_server_start(void);

#endif /* SOMEIP_SERVER_H */
