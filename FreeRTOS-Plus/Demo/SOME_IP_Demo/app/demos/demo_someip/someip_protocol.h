#pragma once
#include <stdint.h>
#include "FreeRTOS_IP.h"

#ifndef SOMEIP_PROTOCOL_H
#define SOMEIP_PROTOCOL_H
/* SOME/IP protocol constants */
#define SOMEIP_PROTOCOL_VERSION   0x01
#define SOMEIP_INTERFACE_VERSION  0x01

#define SOMEIP_MSG_REQUEST        0x00
#define SOMEIP_MSG_RESPONSE       0x80

#define SOMEIP_RETURN_OK          0x00

#define SOMEIP_SERVICE_ID         0x1234
#define SOMEIP_METHOD_ID          0x0001

/* SOME/IP Method IDs */
#define SOMEIP_METHOD_GET_TEMPERATURE   0x0001
#define SOMEIP_METHOD_GET_RPM           0x0002
#define SOMEIP_METHOD_GET_STATUS        0x0003

/* SOME/IP Return Codes */
#define SOMEIP_E_OK                     0x00
#define SOMEIP_E_UNKNOWN_METHOD         0x01
#define SOMEIP_E_NOT_AVAILABLE          0x02
/* ---- TYPE DEFINITIONS FIRST ---- */
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

/* ---- FUNCTION DECLARATIONS AFTER ---- */
void someip_FreeRTOS_hton(someip_header_t *h);
void someip_FreeRTOS_ntohs(someip_header_t *h);
        
#endif /* SOMEIP_PROTOCOL_H */