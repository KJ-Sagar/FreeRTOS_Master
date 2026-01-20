#pragma once

#include "someip_types.h"
#include "someip_errors.h"

/*
 * SOME/IP processing context
 * Transport-agnostic
 */
typedef struct
{
    /* Request */
    someip_header_t *request;
    uint8_t         *request_payload;
    uint32_t         request_len;

    /* Response */
    someip_header_t *response;
    uint8_t         *response_payload;
    uint32_t         response_len;
} someip_context_t;

/*
 * Core SOME/IP message processor
 * (implementation comes later)
 */
someip_return_code_t someip_process_message(someip_context_t *ctx);
