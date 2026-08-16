# Bounded Messaging

This is the bounded-storage contract shipped in release 1.0.0.

`DispatchPort` owns two fixed-capacity queue buffers: an incoming queue and the
current dispatch batch. Both buffers reserve their complete capacity when the
port is constructed. Each queue entry stores its payload in an aligned inline
byte array, so publishing and dispatching do not allocate message payloads.

## Configuration

```cpp
using namespace rtos::messaging;

DispatchPort port{
    "control",
    QueueConfiguration{
        .depth = 32,
        .maximumMessageSize = 64,
        .fullPolicy = QueueFullPolicy::dropOldest,
    }
};
```

The framework supports payloads up to `maximumSupportedMessageSize` bytes. A
message must be trivially copyable and may not require alignment greater than
`std::max_align_t`. Queue configuration is immutable after construction.

## Full Queue Policies

- `rejectNewest` leaves the queue unchanged and returns `rejectedQueueFull`.
- `dropNewest` leaves the queue unchanged and returns `droppedNewest`.
- `dropOldest` removes the oldest entry, queues the new message, and returns
  `queuedAfterDroppingOldest`.

`send()` returns `SendResult`; existing publishers may ignore it, while
reliability-sensitive models can call `accepted(result)`. `QueueStatistics`
reports capacity, pending count, high-water mark, full-queue outcomes, and
oversize rejections. The same limits and policies apply to received transport
messages before they enter the local dispatch queue.

The subscription registry and host IPC envelopes still use host-oriented STL
storage. The bounded queue removes per-message payload allocation and establishes
the size/depth contract used by the FreeRTOS adapter; later embedded milestones
can replace subscription setup and transport envelopes without changing models.

## Determinism Notes

Queue memory is reserved during port construction. `dropOldest` performs a
bounded shift proportional to configured depth; the other full policies return
without moving queued entries. Host subscription and IPC setup may still use
dynamic allocation, as listed in the 1.0.0 compatibility limits.
