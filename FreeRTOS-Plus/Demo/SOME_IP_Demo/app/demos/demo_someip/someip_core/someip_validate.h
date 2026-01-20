#ifndef SOMEIP_VALIDATE_H
#define SOMEIP_VALIDATE_H

#include "FreeRTOS.h"
#include "someip_types.h"

BaseType_t someip_validate_header(const someip_header_t *hdr);

#endif
