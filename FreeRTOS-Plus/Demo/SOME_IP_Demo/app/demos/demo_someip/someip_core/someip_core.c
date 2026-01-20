#include "someip_types.h"

static inline uint32_t someip_make_request_id(
    uint16_t client_id,
    uint16_t session_id)
{
    return ((uint32_t)client_id << 16) | session_id;
}
