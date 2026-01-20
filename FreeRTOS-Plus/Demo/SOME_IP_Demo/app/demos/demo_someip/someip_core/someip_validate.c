#include "someip_validate.h"

/*
 * Basic SOME/IP header validation
 * This will be extended incrementally
 */
someip_return_code_t someip_validate_header(
    const someip_header_t *hdr)
{
    if (hdr == NULL)
        return SOMEIP_E_MALFORMED_MESSAGE;

    /* Protocol version check (AUTOSAR mandates 0x01) */
    if (hdr->protocol_version != 0x01)
        return SOMEIP_E_MALFORMED_MESSAGE;

    /* Message type sanity */
    switch (hdr->message_type)
    {
        case SOMEIP_MSG_REQUEST:
        case SOMEIP_MSG_NOTIFICATION:
            break;

        default:
            return SOMEIP_E_MALFORMED_MESSAGE;
    }

    return SOMEIP_E_OK;
}
