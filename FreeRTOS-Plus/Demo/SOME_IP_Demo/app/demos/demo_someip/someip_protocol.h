#pragma once
#ifndef SOMEIP_PROTOCOL_H
#define SOMEIP_PROTOCOL_H
#include <stdint.h>
#include "FreeRTOS_IP.h"

/* =========================
 * SOME/IP MESSAGE TYPES
 * ========================= */
#define SOMEIP_MSG_REQUEST        0x00
#define SOMEIP_MSG_NOTIFICATION   0x02
#define SOMEIP_MSG_RESPONSE       0x80
#define SOMEIP_MSG_ERROR          0x81

/* =========================
 * SOME/IP RETURN CODES
 * ========================= */
#define SOMEIP_RET_OK                     0x00
#define SOMEIP_RET_E_UNKNOWN_SERVICE      0x02
#define SOMEIP_RET_E_UNKNOWN_METHOD       0x03
#define SOMEIP_RET_E_MALFORMED_MESSAGE    0x09

/* =========================
 * SOME/IP SERVICE IDS
 * ========================= */
#define SOMEIP_SERVICE_SENSOR     0x1001
#define SOMEIP_SERVICE_ENGINE     0x1002
#define SOMEIP_SERVICE_SYSTEM     0x1003

/* Service Discovery (Lite) */
#define SOMEIP_SERVICE_SD         0xFFFF

/* =========================
 * SENSOR SERVICE METHODS
 * ========================= */
#define SOMEIP_METHOD_GET_TEMPERATURE  0x0001
#define SOMEIP_METHOD_GET_HUMIDITY     0x0002

/* =========================
 * ENGINE SERVICE METHODS
 * ========================= */
#define SOMEIP_METHOD_GET_RPM          0x0010
#define SOMEIP_METHOD_GET_TORQUE       0x0011

/* =========================
 * SYSTEM SERVICE METHODS
 * ========================= */
#define SOMEIP_METHOD_GET_STATUS       0x0020
#define SOMEIP_METHOD_GET_UPTIME       0x0021

/* =========================
 * SOME/IP HEADER
 * ========================= */
typedef struct
{
    uint16_t service_id;
    uint16_t method_id;
    uint32_t length;
    uint8_t  message_type;
    uint8_t  return_code;
    uint16_t client_id;
    uint16_t session_id;
} someip_header_t;

/* =========================
 * BYTE ORDER HELPERS
 * ========================= */
void someip_hton_header(someip_header_t *hdr);
void someip_ntoh_header(someip_header_t *hdr);

#endif /* SOMEIP_PROTOCOL_H */
