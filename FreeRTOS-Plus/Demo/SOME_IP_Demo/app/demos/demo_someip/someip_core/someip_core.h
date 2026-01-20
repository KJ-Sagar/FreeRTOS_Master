#ifndef SOMEIP_CORE_H
#define SOMEIP_CORE_H

#include "FreeRTOS.h"
#include "someip_types.h"

#define SOMEIP_MAX_SERVICES 8

/*
 * Service handler prototype.
 * - Writes response payload and length
 * - Sets return code
 * - Returns pdPASS on success, pdFAIL otherwise
 */
typedef BaseType_t (*someip_service_handler_t)(
    uint16_t service_id,
    uint16_t method_id,
    const uint8_t *req_payload,
    uint32_t req_len,
    uint8_t *resp_payload,
    uint32_t *resp_len,
    someip_return_code_t *ret_code
);

/*
 * Register a service handler for a given service ID.
 * Returns pdPASS on success, pdFAIL if registry is full or invalid.
 */
BaseType_t someip_register_service(
    uint16_t service_id,
    someip_service_handler_t handler);

/*
 * Lookup a registered service handler by service ID.
 * Returns handler pointer or NULL if not found.
 */
someip_service_handler_t someip_find_service(uint16_t service_id);

#endif /* SOMEIP_CORE_H */
