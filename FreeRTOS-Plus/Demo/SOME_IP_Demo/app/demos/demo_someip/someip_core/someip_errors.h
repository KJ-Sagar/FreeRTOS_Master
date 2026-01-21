#pragma once
#include <stdint.h>
/*
 * SOME/IP return codes (AUTOSAR-aligned subset)
 */
typedef enum
{
    SOMEIP_E_OK                = 0x00,
    SOMEIP_E_UNKNOWN_SERVICE   = 0x02,
    SOMEIP_E_UNKNOWN_METHOD    = 0x03,
    SOMEIP_E_NOT_READY         = 0x04,
    SOMEIP_E_MALFORMED_MESSAGE = 0x09
} someip_return_code_t;
