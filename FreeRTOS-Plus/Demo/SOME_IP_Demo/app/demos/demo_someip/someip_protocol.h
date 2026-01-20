#ifndef SOMEIP_PROTOCOL_H
#define SOMEIP_PROTOCOL_H

#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "someip_core/someip_types.h"

/*
 * NOTE:
 *  - This file MUST NOT redefine message types, return codes,
 *    or the SOME/IP header.
 *  - All canonical protocol definitions live in someip_types.h
 *  - This file only provides byte-order helpers.
 */

/* =========================================================
 * Byte order helpers for SOME/IP header
 * ========================================================= */
void someip_hton_header(someip_header_t *hdr);
void someip_ntoh_header(someip_header_t *hdr);

#endif /* SOMEIP_PROTOCOL_H */
