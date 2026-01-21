#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "someip_protocol.h"

#define PORT 30509

int main(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&addr, sizeof(addr));
    listen(s, 1);

    printf("Dummy SOME/IP server listening...\n");

    int c = accept(s, NULL, NULL);

    someip_header_t req;
    recv(c, &req, sizeof(req), MSG_WAITALL);
    someip_ntoh(&req);

    printf("Received SOME/IP request:\n");
    printf("  Service ID: 0x%04X\n", req.service_id);
    printf("  Method  ID: 0x%04X\n", req.method_id);

    someip_header_t resp = req;
    resp.message_type = SOMEIP_MSG_RESPONSE;
    resp.length = sizeof(int32_t);
    someip_hton(&resp);

    int32_t temperature = htonl(251); // 25.1 C

    send(c, &resp, sizeof(resp), 0);
    send(c, &temperature, sizeof(temperature), 0);

    close(c);
    close(s);
    return 0;
}
