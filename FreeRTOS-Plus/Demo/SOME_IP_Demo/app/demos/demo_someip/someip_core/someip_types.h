#ifndef SOMEIP_TYPES_H
#define SOMEIP_TYPES_H

#include <stdint.h>

/* =========================================================
 * SOME/IP Versions
 * ========================================================= */
#define SOMEIP_PROTOCOL_VERSION   0x01
#define SOMEIP_INTERFACE_VERSION  0x01

/* =========================================================
 * SOME/IP Length Semantics
 *
 * Length = everything AFTER service_id & method_id
 * (client_id + session_id + protocol_version +
 *  interface_version + message_type + return_code + payload)
 * ========================================================= */
#define SOMEIP_HEADER_PAYLOAD_OFFSET 8U

#define SOMEIP_LENGTH_FIELD(payload_len) \
    ((payload_len) + SOMEIP_HEADER_PAYLOAD_OFFSET)

/* =========================================================
 * SOME/IP Message Types
 * ========================================================= */
typedef enum
{
    SOMEIP_MSG_REQUEST      = 0x00,
    SOMEIP_MSG_NOTIFICATION = 0x02,
    SOMEIP_MSG_RESPONSE     = 0x80,
    SOMEIP_MSG_ERROR        = 0x81
} someip_message_type_t;

/* =========================================================
 * SOME/IP Return Codes (subset, expandable)
 * ========================================================= */
typedef enum
{
    SOMEIP_RET_OK                  = 0x00,
    SOMEIP_RET_E_UNKNOWN_SERVICE   = 0x02,
    SOMEIP_RET_E_UNKNOWN_METHOD    = 0x03,
    SOMEIP_RET_E_MALFORMED_MESSAGE = 0x09
} someip_return_code_t;

/* =========================================================
 * SOME/IP Header
 * ========================================================= */
#if defined(__GNUC__)
#define SOMEIP_PACKED __attribute__((packed))
#else
#define SOMEIP_PACKED
#warning "Packed attribute not defined for this compiler"
#endif

typedef struct SOMEIP_PACKED
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

/* =========================================================
 * Compile-time sanity check
 * ========================================================= */
#define SOMEIP_HEADER_SIZE 16U

typedef char someip_header_size_check[
    (sizeof(someip_header_t) == SOMEIP_HEADER_SIZE) ? 1 : -1
];

#endif /* SOMEIP_TYPES_H */
