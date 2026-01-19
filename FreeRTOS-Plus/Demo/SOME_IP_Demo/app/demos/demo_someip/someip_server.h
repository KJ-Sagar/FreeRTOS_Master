#ifndef SOMEIP_SERVER_H
#define SOMEIP_SERVER_H

#include "FreeRTOS_Sockets.h"
#include "someip_protocol.h"

#define SOMEIP_SERVER_PORT        30509
#define SOMEIP_RX_BUFFER_SIZE     256
#define SOMEIP_TX_BUFFER_SIZE     256

void someip_server_start(void);

#endif /* SOMEIP_SERVER_H */
