#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "someip_protocol.h"

#define SERVER_IP   "127.0.0.1"   // or TAP IP
#define SERVER_PORT 30509

// Define the SOMEIP constants if not already defined in someip_protocol.h
#define SOMEIP_SERVICE_ID 0x1234  // Example service ID; adjust as needed
#define SOMEIP_METHOD_ID 0x5678   // Example method ID; adjust as needed
//#define SOMEIP_PROTOCOL_VERSION 1
//#define SOMEIP_INTERFACE_VERSION 1
#define SOMEIP_MSG_REQUEST 0x00

int main(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = FreeRTOS_htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    someip_header_t req = {0};
    req.service_id = SOMEIP_SERVICE_ID;
    req.method_id  = SOMEIP_METHOD_ID;
    req.client_id  = 1;
    req.session_id = 1;
    req.protocol_version = SOMEIP_PROTOCOL_VERSION;
    req.interface_version = SOMEIP_INTERFACE_VERSION;
    req.message_type = SOMEIP_MSG_REQUEST;
    req.length = 0;

    someip_hton(&req);
    send(sock, &req, sizeof(req), 0);

    someip_header_t resp;
    recv(sock, &resp, sizeof(resp), MSG_WAITALL);
    someip_FreeRTOS_ntohs(&resp);

    int32_t temperature;
    recv(sock, &temperature, sizeof(temperature), MSG_WAITALL);
    temperature = FreeRTOS_ntohsl(temperature);

    printf("Temperature: %.1f C\n", temperature / 10.0);

    close(sock);
    return 0;
}