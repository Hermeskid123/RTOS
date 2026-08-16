# ROS Messaging

ROS Messaging provides deferred, strongly typed publish/subscribe delivery
through `rtos::messaging::DispatchPort`.

```cpp
#include "messages/MotorCommand.hpp"
#include "rtos/messaging/DispatchPort.hpp"

rtos::messaging::DispatchPort port;

port.subscribe<rtos::messages::MotorCommand>(
    [](const rtos::messages::MotorCommand& command)
    {
        // Process the command during dispatch.
    }
);

port.send(rtos::messages::MotorCommand{1500});
const auto report = port.dispatchAll();
```

`send()` owns a copy or moved instance of the message and never invokes callbacks.
`dispatchAll()` routes the current batch to every subscriber registered for the
exact C++ message type. Messages published by a callback remain queued until the
next call to `dispatchAll()`.

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

Subscription removal is intentionally deferred to the subscription-lifetime
milestone.

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

The current implementation is single-threaded and uses dynamic allocation for
host development. Subscription lifetime and bounded embedded storage are planned
for later milestones. ROS Messaging is an internal name and does not imply
compatibility with ROS 1 or ROS 2.

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
CLI `ports` command displays this topology even before the simulation starts.
