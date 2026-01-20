#ifndef SOMEIP_VALIDATE_H
#define SOMEIP_VALIDATE_H

#include "FreeRTOS.h"
#include "someip_types.h"

/*
 * Validate SOME/IP header before dispatch.
 * Returns pdPASS if header is valid, pdFAIL otherwise.
 */
BaseType_t someip_validate_header(const someip_header_t *hdr);

#endif /* SOMEIP_VALIDATE_H */
