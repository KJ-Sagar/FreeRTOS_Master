#ifndef HEARTBEAT_SERVICE_H
#define HEARTBEAT_SERVICE_H

/*
 * Heartbeat service public interface.
 * This header must NOT contain implementation code.
 * Service logic lives in heartbeat_service.c only.
 */

/*
 * Initialize and register the Heartbeat SOME/IP service.
 * Must be called before someip_server_start().
 */
void HeartbeatService_Init(void);

#endif /* HEARTBEAT_SERVICE_H */
