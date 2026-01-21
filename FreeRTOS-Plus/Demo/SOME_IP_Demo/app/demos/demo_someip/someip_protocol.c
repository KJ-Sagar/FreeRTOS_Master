#include "someip_protocol.h"
#include "someip_core/someip_types.h"

/*
 * Convert SOME/IP header fields from host to network byte order.
 * Only multi-byte numeric fields are converted.
 */
void someip_hton_header(someip_header_t *hdr)
{
    if (hdr == NULL)
        return;

    hdr->service_id = FreeRTOS_htons(hdr->service_id);
    hdr->method_id  = FreeRTOS_htons(hdr->method_id);
    hdr->length     = FreeRTOS_htonl(hdr->length);
    hdr->client_id  = FreeRTOS_htons(hdr->client_id);
    hdr->session_id = FreeRTOS_htons(hdr->session_id);
}

/*
 * Convert SOME/IP header fields from network to host byte order.
 */
void someip_ntoh_header(someip_header_t *hdr)
{
    if (hdr == NULL)
        return;

    hdr->service_id = FreeRTOS_ntohs(hdr->service_id);
    hdr->method_id  = FreeRTOS_ntohs(hdr->method_id);
    hdr->length     = FreeRTOS_ntohl(hdr->length);
    hdr->client_id  = FreeRTOS_ntohs(hdr->client_id);
    hdr->session_id = FreeRTOS_ntohs(hdr->session_id);
}
