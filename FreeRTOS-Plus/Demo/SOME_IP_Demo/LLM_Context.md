
# LLM CONTEXT — FreeRTOS SOME/IP Demo (QEMU + Linux Client)

## 1. Project Overview

This project implements a **SOME/IP server on FreeRTOS** running under **QEMU**, with a **Linux-based Python client** used for:
- Service Discovery (SD)
- TCP SOME/IP requests
- Subscription-based notifications (Heartbeat service)

The goal is to build a **spec-faithful, debuggable, and interoperable SOME/IP reference implementation**, not a toy demo.

---

## 2. Runtime Architecture

### Server Side (FreeRTOS / QEMU)
- TCP SOME/IP server on port `30509`
- UDP Service Discovery server on port `30490`
- Services:
  - Heartbeat (`0x1234`)
  - Sensor (`0x1001`)
  - Engine (`0x1002`)
- Tasks:
  - `someip_server_task` — TCP accept + request/response
  - `someip_notification_task` — periodic heartbeat notifications
  - `sd_udp_task` — unicast FindService → Offer handling

### Client Side (Linux / Python)
- Sends unicast FindService
- Connects via TCP
- Subscribes to heartbeat notifications
- Periodically invokes heartbeat method
- Validates SOME/IP framing strictly

---

## 3. Files Involved

### Core Server Files
- `someip_server.c`
- `someip_protocol.c / .h`
- `someip_types.h`
- `heartbeat_service.c`
- `sd_udp_server.c / .h`
- `demo_someip.c`

### Client
- `someip_UDP.py`

---

## 4. Key Design Decisions

### 4.1 Service Discovery
- Uses **unicast FindService**, not broadcast
- Client sends a dummy byte (`0x00`)
- Server replies with compact offer:
```

[service_id_1][service_id_2][service_id_3]

```
- TTL and SOME/IP-SD TLVs are intentionally omitted (simplified model)

### 4.2 SOME/IP Length Semantics
- SOME/IP `length` field = **bytes after service_id + method_id**
- Minimum valid value = `8`
- Header size = `16 bytes`
- Payload offset = `8 bytes`

---

## 5. The Core Bug (Root Cause of Weeks of Failure)

### Symptom (Client)
```

[RESP] Heartbeat ACK
[RX] Dropping malformed frame (length < 8)
(repeats forever)

```

### Symptom (Server)
```

SOME/IP: Client connected
SOME/IP: Heartbeat notification sent

````

### Actual Root Cause
In `someip_server_task()`:

- `hdr.length` was calculated using **response payload length**
- `FreeRTOS_send()` used **request payload length**

This caused:
- TCP stream desynchronization
- Client reading partial headers
- Infinite malformed frame drops
- Eventual connection reset

### Buggy Pattern
```c
hdr.length = SOMEIP_HEADER_PAYLOAD_OFFSET + resp_len;
...
size_t frame_len = sizeof(someip_header_t) + payload_len; // WRONG
````

### Correct Pattern

```c
hdr.length = SOMEIP_HEADER_PAYLOAD_OFFSET + resp_len;
size_t frame_len = sizeof(someip_header_t) + resp_len;     // CORRECT
```

This bug is subtle, compiles cleanly, and only manifests at runtime.

---

## 6. Why This Was Hard to Fix

* TCP does not preserve message boundaries
* First response (ACK) worked by coincidence
* Notifications poisoned the stream afterward
* Client behavior was correct the entire time
* Multiple misleading secondary errors appeared:

  * macro name mismatches
  * header redefinitions
  * missing includes
  * socket timeouts
* The real issue was **one incorrect byte count**

---

## 7. Current Correct Behavior (Expected)

### Client Output

```
[SD] Services offered:
  Service ID: 0x1234
  Service ID: 0x1001
  Service ID: 0x1002
[CLIENT] Connected
[RESP] Heartbeat ACK
[NOTIFY] Heartbeat alive = 1
[NOTIFY] Heartbeat alive = 1
[RESP] Heartbeat = 1
```

### Server Output

```
SOME/IP: Listening on 30509
SOME/IP: Client connected
SOME/IP: Heartbeat notification sent
```

No malformed frames. No disconnects.

---

## 8. Important Conventions Used

### Method IDs

* `SOMEIP_METHOD_SUBSCRIBE   = 0x0100`
* `SOMEIP_METHOD_UNSUBSCRIBE = 0x0101`
* `METHOD_HEARTBEAT          = 0x0001`

Compatibility macros exist to handle historical naming mismatches.

---

## 9. Implementation Rules (Do Not Violate)

1. **Always compute frame length from actual payload sent**
2. `hdr.length` and `FreeRTOS_send()` size must agree
3. Never reuse request payload length for responses
4. Notifications must be framed independently
5. Client parsing is strict by design — do not relax it

---

## 10. Current Project Phase

**Phase 2A — COMPLETE**

* TCP SOME/IP server
* Subscription model
* Notifications
* SD interoperability
* Deterministic framing

---

## 11. Next Planned Phases

### Phase 2B

* Multiple concurrent clients
* Per-client session IDs
* Resubscribe on reconnect

### Phase 3

* SOME/IP-SD TTL handling
* Event groups
* Multiple event types

---

## 12. Mental Model for Future LLMs

If you see:

```
Dropping malformed frame (length < 8)
```

Immediately suspect:

> **Frame length mismatch on the server side**

Not Python. Not sockets. Not TCP.

---

## End of Context

```

---

### Final note (important)

This file does **exactly** what you asked:
- An LLM can read *only this*
- Understand the full history
- Know what broke, why, and how it was fixed
- Continue development without re-debugging old issues

If you want, next we can:
- Add a **sequence diagram**
- Or create a **“debug checklist”** file
- Or move straight into **Phase 2B design**

You’ve earned the next phase.
```
