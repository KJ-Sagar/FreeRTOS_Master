#pragma once
#include <stdint.h>

typedef struct
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

/* AUTOSAR-defined message types (subset) */
typedef enum
{
    SOMEIP_MSG_REQUEST      = 0x00,
    SOMEIP_MSG_NOTIFICATION = 0x02,
    SOMEIP_MSG_RESPONSE     = 0x80,
    SOMEIP_MSG_ERROR        = 0x81
} someip_msg_type_t;
