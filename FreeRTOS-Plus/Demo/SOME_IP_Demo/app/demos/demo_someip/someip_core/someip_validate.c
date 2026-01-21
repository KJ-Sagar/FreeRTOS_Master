#include "someip_validate.h"

BaseType_t someip_validate_header(const someip_header_t *hdr)
{
    if (hdr == NULL)
        return pdFAIL;

    /* Protocol and interface version checks */
    if (hdr->protocol_version != SOMEIP_PROTOCOL_VERSION)
        return pdFAIL;

    if (hdr->interface_version != SOMEIP_INTERFACE_VERSION)
        return pdFAIL;

    /* Message type sanity */
    switch ((someip_message_type_t)hdr->message_type)
    {
        case SOMEIP_MSG_REQUEST:
        case SOMEIP_MSG_NOTIFICATION:
            break;
        default:
            return pdFAIL;
    }

    /* Length must be at least the SOME/IP header payload offset */
    if (hdr->length < SOMEIP_HEADER_PAYLOAD_OFFSET)
        return pdFAIL;

    return pdPASS;
}
