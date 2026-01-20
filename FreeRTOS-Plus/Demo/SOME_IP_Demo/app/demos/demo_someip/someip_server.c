#include "someip_server.h"
#include "someip_protocol.h"
#include "someip_core/someip_core.h"
#include "someip_core/someip_validate.h"

#include "FreeRTOS.h"
#include "task.h"
#include "string.h"

/* transport code omitted for clarity */

static void someip_server_task(void *arg);

void someip_server_start(void)
{
    xTaskCreate(
        someip_server_task,
        "SOMEIP_SERVER",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL);
}

static void someip_server_task(void *arg)
{
    (void)arg;

    uint8_t rx_buf[256];
    uint8_t tx_buf[256];

    for (;;)
    {
        /* recv socket data into rx_buf */

        someip_header_t hdr;
        memcpy(&hdr, rx_buf, sizeof(hdr));
        someip_ntoh_header(&hdr);

        if (someip_validate_header(&hdr) != pdPASS)
        {
            hdr.return_code = SOMEIP_RET_E_MALFORMED_MESSAGE;
            goto send_response;
        }

        someip_service_handler_t handler =
            someip_find_service(hdr.service_id);

        uint32_t payload_len = 0;
        someip_return_code_t ret = SOMEIP_RET_OK;

        if (!handler ||
            handler(hdr.service_id,
                    hdr.method_id,
                    rx_buf + sizeof(hdr),
                    hdr.length - SOMEIP_HEADER_PAYLOAD_OFFSET,
                    tx_buf + sizeof(hdr),
                    &payload_len,
                    &ret) != pdPASS)
        {
            ret = SOMEIP_RET_E_UNKNOWN_METHOD;
        }

        hdr.return_code = ret;
        hdr.message_type =
            (ret == SOMEIP_RET_OK)
                ? SOMEIP_MSG_RESPONSE
                : SOMEIP_MSG_ERROR;

send_response:
        hdr.length = SOMEIP_LENGTH_FIELD(payload_len);
        someip_hton_header(&hdr);
        memcpy(tx_buf, &hdr, sizeof(hdr));

        /* send tx_buf */
    }
}
