#ifndef TCP_HEARTBEAT_DEMO_H
#define TCP_HEARTBEAT_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the TCP heartbeat task.
 * Must be called only after the network is UP.
 */
void vStartTCPHeartbeatDemo( void );

#ifdef __cplusplus
}
#endif

#endif /* TCP_HEARTBEAT_DEMO_H */
