#pragma once

#include "someip_types.h"
#include "someip_errors.h"

/*
 * Validate a SOME/IP header according to core rules.
 * Transport-agnostic.
 */
someip_return_code_t someip_validate_header(
    const someip_header_t *hdr);
