# ROS Messaging

This document describes the messaging contract shipped in release 1.0.0.

ROS Messaging provides deferred, strongly typed publish/subscribe delivery
through `rtos::messaging::DispatchPort`.

```cpp
#include "messages/MotorCommand.hpp"
#include "rtos/messaging/DispatchPort.hpp"

rtos::messaging::DispatchPort port;

auto subscription = port.subscribe<rtos::messages::MotorCommand>(
    [](const rtos::messages::MotorCommand& command)
    {
        // Process the command during dispatch.
    }
);

port.send(rtos::messages::MotorCommand{1500});
const auto report = port.dispatchAll();
```

`send()` copies the trivially-copyable payload into queue-owned inline storage
and never invokes callbacks.
`dispatchAll()` routes the current batch to every subscriber registered for the
exact C++ message type. Messages published by a callback remain queued until the
next call to `dispatchAll()`.

Queue depth, maximum payload size, and full-queue behavior are configured when a
`DispatchPort` is constructed. Pending messages use preallocated inline payload
slots, and `send()` reports whether a message was queued, rejected, or replaced
an older entry. See [`bounded_messaging.md`](bounded_messaging.md).

## Dispatch Semantics

- Messages are delivered in publication order.
- Subscribers for a type are invoked in registration order.
- Each message is delivered once to every matching subscriber.
- Messages with no matching subscriber are discarded during dispatch.
- An empty dispatch has no effect.
- A nested `dispatchAll()` call from a callback has no effect, so messages sent
  from callbacks cannot cross the current dispatch boundary.

## Subscription Registry

`DispatchPort` owns a `SubscriptionRegistry` that maps exact C++ message types to
their callbacks. Multiple callbacks may be registered for the same message type.
The current number of callbacks for a type can be queried when needed:

```cpp
const auto motorSubscribers =
    port.subscriberCount<rtos::messages::MotorCommand>();
```

## Subscription Lifetime

`subscribe<Message>()` returns a move-only `SubscriptionHandle`. Retain the
handle for as long as delivery is desired. Calling `reset()` or destroying the
handle unregisters the callback. Handles use weak registry ownership, so
destroying a handle after its dispatch port is also safe.

Models store handles as members whenever callbacks capture `this`. Member
destruction invalidates the subscription before later dispatch can invoke a
callback against the destroyed model. Resetting a handle during dispatch also
marks its shared callback slot inactive, even if the slot is already present in
the current dispatch snapshot.

## Dispatch Reports

Each call to `dispatchAll()` returns a `DispatchReport` containing:

- `messagesDispatched` — messages that had at least one matching subscriber;
- `callbacksInvoked` — total callbacks invoked across all messages;
- `messagesWithoutSubscribers` — messages discarded without delivery;
- `messagesProcessed()` — total handled and unhandled messages in the batch.

Callers may ignore the report when routing diagnostics are not needed.

## Project Messages

- `rtos::messages::MotorCommand` requests a target motor RPM.
- `rtos::messages::MotorStatus` reports the current motor RPM.
- `rtos::messages::SensorData` carries a host-simulation sensor value.

The host simulator runs models in separate processes and connects their dispatch
ports through an IPC transport. Queue operations, routing diagnostics, and
subscription lifetime are thread-safe within the boundaries documented in
[`concurrency.md`](concurrency.md). ROS Messaging is an internal name and does
not imply compatibility with ROS 1 or ROS 2.

Publishing, receiving, diagnostics, subscription changes, and dispatch entry are
thread-safe. Dispatch moves the current queue into a private batch, so concurrent
publishers target the following dispatch boundary. Subscriber callbacks run
without holding the dispatch queue mutex.

## Diagnostics

Each `DispatchPort` has a name and exposes its pending queue depth through
`pendingMessageCount()`. `messageTraffic()` reports every registered or sent
message type, including publisher, subscriber, sent, received, dispatched, and
no-subscriber counts. Received counts represent subscriber callback deliveries,
so one dispatched message can produce multiple received deliveries.
The host CLI uses these diagnostics for its `ports` and `messages` commands.

## Named Model Ports

Models create typed endpoints with an explicit name and direction:

```cpp
auto commandPort = dispatchPort.createPort<MotorCommand>(
    "MotorCommand_port",
    PortDirection::publisher
);
```

Each endpoint receives a sequential port number. `portTopology()` reports its
name, number, message type, publisher port numbers, and subscriber port numbers.
Publisher/subscriber relationships are derived by matching message types. The
CLI `ports` command displays the worker topology after `start sim`.

## Transport And Routing

Every dispatch port reports a `TransportType`: `IN_PROCESS` for local unit-test
and embedded-style routing, or `IPC` for host model workers. Transport envelopes
carry a `RoutingId`, message name, and byte payload. Endpoint routing defaults to
the message type's `defaultRoutingId`; callers may pass an explicit third
argument to `createPort<Message>()` to override it.

The project message defaults are `SensorData=1001`, `MotorCommand=1002`, and
`MotorStatus=1003`. The CLI `ports` command displays the worker PID, transport,
and routing ID for every endpoint.
