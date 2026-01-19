/*
 * FreeRTOS SomeIP Protocol Demo
 *
 * SPDX-License-Identifier: MIT
 *#include "someip_protocol.h"
*/
#include "someip_protocol.h"   // MUST come first
#include "FreeRTOS.h"

void someip_hton_header(someip_header_t *hdr)
{
    hdr->service_id  = FreeRTOS_htons(hdr->service_id);
    hdr->method_id   = FreeRTOS_htons(hdr->method_id);
    hdr->length      = FreeRTOS_htonl(hdr->length);
    hdr->client_id   = FreeRTOS_htons(hdr->client_id);
    hdr->session_id  = FreeRTOS_htons(hdr->session_id);
}

void someip_ntoh_header(someip_header_t *hdr)
{
    hdr->service_id  = FreeRTOS_ntohs(hdr->service_id);
    hdr->method_id   = FreeRTOS_ntohs(hdr->method_id);
    hdr->length      = FreeRTOS_ntohl(hdr->length);
    hdr->client_id   = FreeRTOS_ntohs(hdr->client_id);
    hdr->session_id  = FreeRTOS_ntohs(hdr->session_id);
}
