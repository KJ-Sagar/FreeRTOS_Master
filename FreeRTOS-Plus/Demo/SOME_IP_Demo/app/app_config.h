#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Role selection */
#define APP_ROLE_CLIENT     0
#define APP_ROLE_SERVER     1

/* Demo selection (ONLY ONE should be 1) */
#define DEMO_SOMEIP     1
#define DEMO_HEARTBEAT  0
#define DEMO_ECHO       0


/*Server port*/
#define APP_SERVER_PORT    5001

#endif


//HEARTBEAT DEMO — WHO DOES WHAT
/*| Side                | Role           | Why                               |
| ------------------- | -------------- | --------------------------------- |
| **FreeRTOS (QEMU)** | **TCP CLIENT** | Actively initiates the connection |
| **Linux Host**      | **TCP SERVER** | Passively waits for connections   |
*/


//ECHO DEMO — WHO DOES WHAT
/*| Side                | Role           | Why                               |
| ------------------- | -------------- | --------------------------------- |
| **FreeRTOS (QEMU)** | **TCP SERVER** | Passively waits for connections   |
| **Linux Host**      | **TCP CLIENT** | Actively initiates the connection |
*/  

/*
| Feature         | Heartbeat     | Echo              |
| --------------- | ------------- | ----------------- |
| FreeRTOS role   | **Client**    | **Server**        |
| Linux role      | **Server**    | **Client**        |
| Initiates TCP   | FreeRTOS      | Linux             |
| Listening side  | Linux         | FreeRTOS          |
| Port            | 5001          | 7                 |
| Visible output  | Usually none  | Immediate echo    |
| Primary purpose | Link liveness | TCP functionality |
*/