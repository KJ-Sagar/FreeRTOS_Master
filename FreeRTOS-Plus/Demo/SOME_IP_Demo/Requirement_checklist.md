# Platform Architecture Review

**CAN / Ethernet / SOME-IP Binary**
*(Cortex-M & Cortex-A)*

---

# **PAGE 1 — ARCHITECTURE CHECKLIST**

## Cortex-M (M3 / M4 / M7) — Checklist

**Execution & Memory**

* ☐ Runs without an MMU on FreeRTOS
* ☐ Fully statically linked
* ☐ Task stack sizes fixed and bounded
* ☐ Heap usage bounded and predictable

**Interrupts & Tasks**

* ☐ RX paths interrupt-driven (not polling)
* ☐ ISRs short and non-blocking
* ☐ Only RTOS primitives used for concurrency
* ☐ Each task has a single responsibility

**Timing & Behavior**

* ☐ Message handling latency bounded
* ☐ Periodic actions timer-driven
* ☐ Timing based on RTOS or hardware timers

**Protocol Structure**

* ☐ Protocol logic separate from transport
* ☐ Methods and events clearly distinguished
* ☐ Subscription handled as explicit server state

**Binary Practicalities**

* ☐ Fits within Flash/RAM limits
* ☐ Logging optional and non-blocking
* ☐ No filesystem dependency

---

## Cortex-A (A53 / A72 / etc.) — Checklist

**Execution Model**

* ☐ MMU-aware
* ☐ Execution mode clearly defined
* ☐ Target ABI (AArch32/AArch64) explicit

**Concurrency**

* ☐ Safe on multi-core systems
* ☐ Shared resources properly protected
* ☐ No single-core assumptions

**Scheduling & Performance**

* ☐ CAN latency protected from Ethernet load
* ☐ Scheduling policy explicit where needed

**I/O Model**

* ☐ CAN accessed via kernel interfaces
* ☐ Ethernet via standard networking stack
* ☐ I/O errors handled without crashing

**Binary Basics**

* ☐ OS/lib dependencies explicit and minimal
* ☐ Logging bounded and controlled

---

---

# **PAGE 2+ — DETAILED RATIONALE AND EXPLANATION**

---

## Cortex-M — Detailed Explanations

### Runs without an MMU

Cortex-M systems execute in a flat physical address space with no virtual memory, no page protection, and no process isolation.
Every pointer is real, and any memory corruption affects the entire system.
This requirement ensures the binary does not rely on Linux-style memory safety assumptions.

---

### Fully statically linked

There is no loader or dynamic linker on Cortex-M.
All code must be present and resolved at build time.
This guarantees predictable memory usage and eliminates runtime symbol failures.

---

### Task stack sizes fixed and bounded

FreeRTOS stacks do not grow dynamically.
Stack overflow causes silent memory corruption.
Explicit stack sizing forces disciplined call depth and prevents accidental failure modes.

---

### Heap usage bounded and predictable

Dynamic allocation is allowed only if bounded and well understood.
Unbounded allocation or fragmentation leads to unrecoverable failures.
This is critical for networking stacks and protocol buffers.

---

### RX paths interrupt-driven

Polling wastes CPU cycles and introduces latency.
Interrupt-driven RX ensures responsiveness and determinism, especially under load.

---

### ISRs short and non-blocking

ISRs run with interrupts masked and the scheduler suspended.
Blocking or heavy logic inside ISRs breaks real-time behavior system-wide.

---

### Only RTOS primitives used

RTOS primitives handle priority inheritance and scheduling correctly.
Custom locking or ad-hoc synchronization introduces race conditions and deadlocks.

---

### Single responsibility per task

Tasks with one responsibility are easier to reason about, test, and extend.
This structure scales cleanly as features (e.g., CAN, SD) are added.

---

### Message latency bounded

CAN and SOME/IP systems are latency-sensitive.
Unbounded processing time is a functional failure, not a performance issue.

---

### Periodic actions timer-driven

Events must be time-driven, not traffic-driven.
This preserves correct event semantics and avoids accidental polling behavior.

---

### Timing based on RTOS or hardware timers

Cortex-M systems do not have reliable wall-clock time.
All timing must come from deterministic sources.

---

### Protocol separate from transport

Separating protocol logic from transport enables reuse across TCP, UDP, CAN, and future platforms.
This is the foundation for portability.

---

### Methods vs events clearly separated

REQUEST/RESPONSE and NOTIFICATION have different semantics.
Mixing them collapses the protocol model and breaks AUTOSAR alignment.

---

### Explicit subscription state

Subscription changes server behavior; it is not a transport feature.
Explicit state prevents accidental event flooding and enforces correctness.

---

### Fits within Flash/RAM limits

Memory is a hard constraint on Cortex-M.
Growth must be measured and reviewed continuously.

---

### Logging optional and non-blocking

Logging must never affect protocol timing or correctness.
It must be removable at build time.

---

### No filesystem dependency

Most Cortex-M systems have no filesystem.
Configuration and operation must not depend on files or paths.

---

---

## Cortex-A — Detailed Explanations

### MMU-aware

Cortex-A systems use virtual memory and cache hierarchies.
The binary must behave correctly in an MMU-enabled environment.

---

### Execution mode clearly defined

Behavior differs significantly between bare-metal, kernel-space, and user-space.
The design must explicitly state which model it targets.

---

### ABI explicit

AArch32 vs AArch64 affects pointer size, alignment, and calling conventions.
Ambiguity here leads to subtle, catastrophic bugs.

---

### Safe on multi-core systems

Cortex-A commonly runs SMP.
Concurrency issues that never appear on Cortex-M will surface immediately.

---

### Shared resources protected

Concurrent CAN and Ethernet paths must not race.
Proper locking is mandatory, not optional.

---

### CAN latency protected

Ethernet traffic must not starve CAN processing.
This requires explicit scheduling or prioritization decisions.

---

### Kernel-mediated I/O

Direct hardware access is not allowed in user-space.
The design must respect kernel ownership of devices.

---

### I/O error handling

Driver restarts and transient failures must not crash the application.
Process-level robustness is expected on Cortex-A.

---

### Dependencies explicit

Dynamic systems hide complexity in dependencies.
Explicit dependency control prevents version drift and runtime surprises.

---

### Logging bounded

Unlike Cortex-M, logging is acceptable but must still be controlled.
Unbounded logging becomes a denial-of-service vector.

---

