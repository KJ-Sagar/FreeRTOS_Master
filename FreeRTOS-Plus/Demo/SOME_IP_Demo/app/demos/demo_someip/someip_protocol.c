#include "someip_protocol.h"
#include "FreeRTOS_IP.h"

void someip_hton(someip_header_t *h)
{
    h->service_id = htons(h->service_id);
    h->method_id  = htons(h->method_id);
    h->length     = htonl(h->length);
    h->client_id  = htons(h->client_id);
    h->session_id = htons(h->session_id);
}

void someip_ntoh(someip_header_t *h)
{
    h->service_id = ntohs(h->service_id);
    h->method_id  = ntohs(h->method_id);
    h->length     = ntohl(h->length);
    h->client_id  = ntohs(h->client_id);
    h->session_id = ntohs(h->session_id);
}
