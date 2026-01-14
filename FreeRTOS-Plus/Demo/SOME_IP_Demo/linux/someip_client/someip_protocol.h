#pragma once
#include <stdint.h>

#define SOMEIP_PROTOCOL_VERSION   0x01
#define SOMEIP_INTERFACE_VERSION  0x01

#define SOMEIP_MSG_REQUEST        0x00
#define SOMEIP_MSG_RESPONSE       0x80

#define SOMEIP_RETURN_OK          0x00

typedef struct __attribute__((packed))
{
    uint16_t service_id;
    uint16_t method_id;
    uint32_t length;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t  protocol_version;
    uint8_t  interface_version;
    uint8_t  message_type;
    uint8_t  return_code;
} someip_header_t;

void someip_hton(someip_header_t *h);
void someip_FreeRTOS_ntohs(someip_header_t *h);
