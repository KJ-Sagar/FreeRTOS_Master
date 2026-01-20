#include "someip_validate.h"

BaseType_t someip_validate_header(const someip_header_t *hdr)
{
    if (hdr == NULL)
        return pdFAIL;

    if (hdr->protocol_version != SOMEIP_PROTOCOL_VERSION)
        return pdFAIL;

    if (hdr->interface_version != SOMEIP_INTERFACE_VERSION)
        return pdFAIL;

    if (hdr->message_type != SOMEIP_MSG_REQUEST &&
        hdr->message_type != SOMEIP_MSG_NOTIFICATION)
        return pdFAIL;

    return pdPASS;
}
