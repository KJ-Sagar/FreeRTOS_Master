# SOME/IP over FreeRTOS (QEMU) — Demo Project

## Overview

This project demonstrates a working **SOME/IP (Scalable service-Oriented MiddlewarE over IP)** implementation using:

- **FreeRTOS + FreeRTOS-Plus-TCP** running on **QEMU (ARM Cortex-M3, MPS2)**
- A **Linux host client** communicating over a TAP interface
- **TCP-based SOME/IP messaging**
- Multiple service methods over a **persistent TCP connection**

The demo proves end-to-end communication between a **simulated embedded ECU** and a **host system**, using real TCP sockets and a structured SOME/IP protocol layer.

---

## Features Implemented

- FreeRTOS TCP/IP stack bring-up in QEMU  
- Persistent SOME/IP server running as a FreeRTOS task  
- Linux SOME/IP client using Python sockets  
- Multiple SOME/IP methods over a single TCP session  
- Proper byte-order handling (host ↔ network)  
- Clean task lifecycle and socket management  
- Clear debug logs on both sides  

---

## Supported SOME/IP Methods

| Method ID | Name         | Payload            |
|----------:|-------------|--------------------|
| `0x0001`  | Temperature | `int32` (×10 °C)   |
| `0x0002`  | RPM         | `uint16`           |
| `0x0003`  | Status      | `uint8`            |

**Common parameters**
- Service ID: `0x1234`
- Transport: TCP
- Protocol Version: 1

---

## Project Structure

```

SOME_IP_Demo/
├── app/
│   ├── app_main.c
│   ├── demo_selector.c
│   └── demos/
│       ├── demo_someip/
│       │   ├── demo_someip.c
│       │   ├── someip_server.c
│       │   ├── someip_protocol.c
│       │   └── someip_protocol.h
│       ├── demo_echo.c
│       └── demo_heartbeat.c
│
├── linux/
│   └── someip_client.py
│
├── platform/
│   ├── main.c
│   └── main_networking.c
│
├── scripts/
│   └── TCP_Demo.sh
│
├── Makefile
└── README.md

````

---

## Build Instructions

From the demo root directory:

```bash
make clean
make
````

This generates:

```text
build/freertos_tcp_mps2_demo.axf
```

---

## Running the Demo

### 1. Start QEMU + Networking

From the `scripts/` directory:

```bash
sudo ./TCP_Demo.sh
```

What this script does:

* Creates a TAP interface (`tap0`)
* Assigns host IP `10.0.0.1`
* Launches QEMU with FreeRTOS
* FreeRTOS obtains IP `10.0.0.2`

Expected QEMU output includes:


APP: Starting SOME/IP demo
SOMEIP: Server start requested
SOMEIP: Socket created
SOMEIP: Bound to port 30509
SOMEIP: Listening...
---

### 2. Run the Linux SOME/IP Client

In another terminal:

```bash
cd linux
python3 someip_client.py
```

Expected output:

```text
Connected (persistent session)

Requesting Temperature
Response Header: service=0x1234, method=0x0001
Payload: Temperature = 25.0 °C

Requesting RPM
Response Header: service=0x1234, method=0x0002
Payload: RPM = 3200

Requesting Status
Response Header: service=0x1234, method=0x0003
Payload: Status = 1

Client done
```

---

## Architecture

### High-Level View

```
+---------------------+        TCP (SOME/IP)        +----------------------+
|                     | <------------------------> |                      |
|   Linux Host        |                            |   QEMU (FreeRTOS)    |
|                     |                            |                      |
|  Python Client      |                            |  SOME/IP Server Task |
|  someip_client.py   |                            |                      |
|                     |                            |  FreeRTOS+TCP Stack  |
+----------+----------+                            +----------+-----------+
           |                                                        |
           |                  TAP Interface (tap0)                |
           +--------------------------------------------------------+
```

---

### FreeRTOS Side (QEMU)

* Runs on ARM Cortex-M3 (MPS2)
* Uses FreeRTOS-Plus-TCP
* A dedicated task (`someip_server_task`) handles:

  * `socket()` → `bind()` → `listen()` → `accept()`
  * Blocking `recv()` for SOME/IP headers
  * Method dispatch and response generation
* Supports multiple requests per TCP session

---

### Linux Side

* Python client uses standard TCP sockets
* Establishes one persistent connection
* Sends multiple SOME/IP requests sequentially
* Decodes response headers and payloads
* Closes connection cleanly

---

## Protocol Layer

* `someip_protocol.c/.h` encapsulates:

  * SOME/IP header definition
  * Host ↔ network byte-order conversion
* Transport logic is separated from protocol logic
* Mirrors real automotive middleware layering

---

## Key Design Decisions

* **Persistent TCP connection**

  * Reduced overhead
  * Matches real SOME/IP usage
* **Blocking sockets**

  * Deterministic behavior
  * Simpler task logic
* **Explicit logging**

  * Easy traceability in QEMU
  * Simplifies debugging

---

## What This Demo Proves

* Real FreeRTOS networking (not mocked)
* Correct TCP socket lifecycle handling
* Correct SOME/IP framing and parsing
* Host ↔ embedded interoperability
* Scalable structure for additional services

---

## Possible Extensions

* SOME/IP client task on FreeRTOS (loopback)
* UDP-based SOME/IP
* SOME/IP Service Discovery (SD)
* Wireshark capture and validation
* Reconnect and fault handling

---

## Final Notes

This project is intentionally **minimal but correct**.
It focuses on clarity, correctness, and extensibility — the same priorities used in real embedded networking systems.

