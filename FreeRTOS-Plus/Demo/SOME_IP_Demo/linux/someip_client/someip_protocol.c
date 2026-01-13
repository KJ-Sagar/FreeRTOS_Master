#include "someip_protocol.h"

#include <arpa/inet.h>

void someip_hton(someip_header_t *h)
{
    h->service_id = FreeRTOS_htons(h->service_id);
    h->method_id  = FreeRTOS_htons(h->method_id);
    h->length     = htonl(h->length);
    h->client_id  = FreeRTOS_htons(h->client_id);
    h->session_id = FreeRTOS_htons(h->session_id);
}

void someip_FreeRTOS_ntohs(someip_header_t *h)
{
    h->service_id = FreeRTOS_ntohs(h->service_id);
    h->method_id  = FreeRTOS_ntohs(h->method_id);
    h->length     = FreeRTOS_ntohl(h->length);
    h->client_id  = FreeRTOS_ntohs(h->client_id);
    h->session_id = FreeRTOS_ntohs(h->session_id);
}
