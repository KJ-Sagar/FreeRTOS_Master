#ifndef HEARTBEAT_SERVICE_H
#define HEARTBEAT_SERVICE_H

#include <stdint.h>

#define SERVICE_HEARTBEAT   0x1234
#define METHOD_HEARTBEAT    0x0001

void HeartbeatService_Init(void);

#endif
