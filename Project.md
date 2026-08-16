# C++ RTOS Model Framework & ROS Messaging Specification

**Stack:** C++20, RTOS-compatible C++, STL-compatible abstractions where appropriate, CMake, host-based unit testing, optional FreeRTOS integration

**Execution Environment:** Fully local (no cloud dependencies)

**Objective:**
Build a modular, model-based C++ framework for practicing and developing RTOS concepts including tasks/threads, scheduling, synchronization, parallel processing, message passing, and Publish/Subscribe architectures.

Application functionality shall be implemented primarily as independent **Models**. Models shall plug into an RTOS execution environment through a thin scheduling/integration layer and communicate with one another through a message-oriented subsystem named **ROS Messaging**.

The initial implementation shall be capable of running without an RTOS on a desktop host so that models and messaging behavior can be tested deterministically. RTOS-specific execution shall be added through adapters rather than embedded directly into model implementations.

---

# 1. Overview

The system shall be divided into four primary layers:

```text
┌─────────────────────────────────────────────┐
│              Application Models             │
│                                             │
│ SensorModel   ControlModel   MotorModel     │
│      │             │             │          │
│      └──────── ROS Messaging ─────┘          │
├─────────────────────────────────────────────┤
│             ROS Messaging Layer             │
│                                             │
│ DispatchPort                                │
│ Publishers / Subscribers                    │
│ Routing                                     │
│ Message Queues                              │
├─────────────────────────────────────────────┤
│           Execution / RTOS Layer            │
│                                             │
│ ModelRunner                                 │
│ Tasks / Threads                             │
│ Timing / Scheduling / Priorities            │
├─────────────────────────────────────────────┤
│             Platform / HAL Layer            │
│                                             │
│ Hardware / Drivers / Timers / Host Stubs    │
└─────────────────────────────────────────────┘
```

The fundamental architectural rule is:

> **Models do not know about other models. Models know about messages.**

A model shall not require direct references to other application models to perform inter-model communication.

For example, this should be avoided:

```cpp
motor.setSpeed(controller.getRequestedSpeed());
```

The preferred architecture is:

```cpp
port.send(MotorCommand{requestedSpeed});
```

The receiving model subscribes to `MotorCommand`.

This separation allows models to be independently:

* unit tested;
* moved between RTOS tasks;
* moved between threads;
* scheduled at different rates;
* replaced;
* simulated;
* disabled;
* executed on a desktop host;
* executed on an embedded target.

ROS Messaging is an internal project name and does **not** imply compatibility with ROS 1 or ROS 2.

---

# 2. High-Level Algorithm

The initial system shall use a **frame-driven execution model**.

A simplified execution cycle is:

```text
START FRAME
     │
     ├── Run scheduled Model A
     │
     ├── Run scheduled Model B
     │
     ├── Run scheduled Model C
     │
     │
     │   Models may call:
     │       port.send(...)
     │
     ▼
ROS Messaging
dispatchAll()
     │
     ├── Read pending messages
     ├── Determine message type/topic
     ├── Find subscriptions
     └── Execute subscriber callbacks
     │
     ▼
Timing / Scheduling
     │
     ▼
END FRAME
```

Conceptually:

```cpp
initialize();

while (running)
{
    scheduler.update();

    rosPort.dispatchAll();

    waitForNextFrame();
}
```

Models execute application logic through an `update()` or equivalent lifecycle function.

During execution, models may publish messages:

```cpp
port.send(message);
```

`send()` shall enqueue a message. It shall **not synchronously invoke subscriber callbacks**.

At a defined synchronization point, the execution environment calls:

```cpp
port.dispatchAll();
```

`dispatchAll()` processes messages pending for that dispatch cycle and routes them to matching subscribers.

### Dispatch Boundary

Messages generated while processing callbacks shall not recursively dispatch during the current dispatch pass.

Example:

```text
Frame N

Model
  │
  └── send(A)

dispatchAll()
  │
  └── dispatch A
         │
         └── Subscriber callback
                │
                └── send(B)

Frame N+1

dispatchAll()
  │
  └── dispatch B
```

This establishes a deterministic dispatch boundary and prevents uncontrolled recursive message chains.

A practical implementation may use two queues:

```text
Incoming Queue
     │
     │ swap
     ▼
Dispatch Queue
     │
     └── process messages

Any send() occurring during dispatch
goes into the new Incoming Queue.
```

---

# 3. Technical Constraints

The implementation shall prioritize:

* deterministic behavior;
* bounded execution where practical;
* explicit ownership;
* minimal coupling;
* testability;
* portability;
* thread safety where required;
* RTOS compatibility;
* understandable C++ abstractions over unnecessary complexity.

The core framework shall not depend directly on a specific RTOS.

Application models should not include FreeRTOS-specific APIs such as:

```cpp
vTaskDelay();
xQueueSend();
xSemaphoreTake();
```

RTOS-specific functionality shall instead live in the execution/platform integration layer.

The framework shall initially target C++20.

Exceptions, RTTI, dynamic allocation, and STL usage may initially be permitted for host development, but the architecture shall not require them permanently. Embedded-safe alternatives may be introduced as the project matures.

Models shall use dependency injection for framework services where practical.

Example:

```cpp
explicit MotorModel(DispatchPort& port);
```

Global messaging objects should be avoided.

### 3.1 Concurrency

The initial messaging implementation may be single-threaded.

Later versions shall support concurrent publishers.

Thread safety shall be introduced deliberately rather than assumed.

Potential synchronization mechanisms include:

```text
std::mutex
std::atomic
std::condition_variable

RTOS Mutex
RTOS Semaphore
RTOS Queue
RTOS Event
```

Subscriber callback execution policy shall remain explicit.

The first implementation shall execute callbacks from the context calling:

```cpp
dispatchAll();
```

### 3.2 Memory and Ownership

Message lifetime shall be unambiguous.

The first implementation may copy or move messages into the dispatch queue.

Unsafe storage of references to stack-owned messages is prohibited.

For example, this semantic shall be safe:

```cpp
{
    MotorCommand command{1000};
    port.send(command);
}

// command no longer exists here

port.dispatchAll();
```

The messaging subsystem therefore owns, copies, moves, or otherwise safely preserves the information required for later dispatch.

Long-term embedded implementations should support fixed-capacity storage to avoid unpredictable heap allocation.

## 3.3 Data

Messages shall preferably be represented as strongly typed C++ structures.

Example:

```cpp
struct MotorCommand
{
    int32_t targetRpm;
};

struct MotorStatus
{
    int32_t currentRpm;
};
```

Initial routing should prefer compile-time C++ message types rather than string-only identifiers.

Preferred:

```cpp
port.send(MotorCommand{1500});

port.subscribe<MotorCommand>(callback);
```

Rather than:

```cpp
port.send("motor_command", data);
```

Message structures should preferably contain data rather than behavior.

Messages should be small and serializable where practical so that future transports can move them between threads, processors, devices, or simulation environments.

A future common message header may contain:

```cpp
struct MessageHeader
{
    MessageId id;
    Timestamp timestamp;
    SourceId source;
    SequenceNumber sequence;
};
```

This header is not required for the first milestone.

---

# 4. Model Specification

A **Model** represents an independently executable application component.

Examples include:

```text
SensorModel
MotorModel
ControlModel
NavigationModel
TelemetryModel
HealthModel
LoggerModel
```

Models should not depend directly upon concrete implementations of other models.

A model lifecycle is:

```cpp
class BaseModel
{
public:
    virtual ~BaseModel() = default;

    virtual ControlStatus initialize() = 0;
    virtual ControlStatus begin() = 0;
    virtual ControlStatus freeze() = 0;
    virtual ControlStatus operate() = 0;
    virtual ControlStatus terminate() = 0;
    virtual ControlStatus status() const = 0;
};
```

`initialize()` is called immediately after construction, before execution;
`begin()` starts or resumes the model; `operate()` performs one scheduled cycle;
`freeze()` pauses the model; and `terminate()` performs final teardown. Each
operation returns `STOPPED`, `RUNNING`, or `TERMINATED` for reporting to the
execution environment.

The exact interface may evolve if compile-time polymorphism proves preferable.

Models receive required framework dependencies through construction or initialization.

Example:

```cpp
class MotorModel : public BaseModel
{
public:
    MotorModel(DispatchPort& port, Logger& logger)
        : port_(port), logger_(logger)
    {
    }

    ControlStatus initialize() override
    {
        port_.subscribe<MotorCommand>(
            [this](const MotorCommand& message)
            {
                onMotorCommand(message);
            });
        return status_;
    }

    ControlStatus begin() override
    {
        status_ = ControlStatus::running;
        logger_.log(LogLevel::debug, "MotorModel", "BEGIN", "started");
        return status_;
    }

    ControlStatus freeze() override
    {
        status_ = ControlStatus::stopped;
        return status_;
    }

    ControlStatus operate() override
    {
        if (status_ != ControlStatus::running) {
            return status_;
        }

        MotorStatus status{
            .currentRpm = currentRpm_
        };

        port_.send(status);
        logger_.log(LogLevel::debug, "MotorModel", "TX", "MotorStatus sent");
        return status_;
    }

    ControlStatus terminate() override
    {
        status_ = ControlStatus::terminated;
        return status_;
    }

    ControlStatus status() const override { return status_; }

private:
    void onMotorCommand(const MotorCommand& message)
    {
        targetRpm_ = message.targetRpm;
        logger_.log(LogLevel::debug, "MotorModel", "RX", "MotorCommand received");
    }

    DispatchPort& port_;
    Logger& logger_;

    int32_t currentRpm_{};
    int32_t targetRpm_{};
    ControlStatus status_{ControlStatus::stopped};
};
```

## Architecture

A model conceptually consists of:

```text
             ROS Messaging
                  │
          subscribed messages
                  │
                  ▼
        ┌──────────────────┐
        │      Model       │
        │                  │
        │ callbacks        │
        │ state            │
        │ update()         │
        │ algorithms       │
        └────────┬─────────┘
                 │
              send()
                 │
                 ▼
             ROS Messaging
```

Models therefore have three primary responsibilities:

```text
INPUT
    Subscription callbacks

PROCESSING
    Model state
    Algorithms
    operate()

OUTPUT
    ROS Messaging send()
```

RTOS scheduling shall be external:

```text
RTOS Task
    │
    ▼
ModelRunner
    │
    ▼
model.operate()
```

This permits:

```text
One Model  → One Task

Multiple Models → One Task

One Model → Desktop simulation loop

One Model → Unit test harness
```

without rewriting the model itself.

---

# 5. Protocol

The internal Pub/Sub subsystem shall be named:

**ROS Messaging**

Its primary abstraction shall be the:

**DispatchPort**

A DispatchPort owns or manages:

```text
DispatchPort
    │
    ├── Subscription Registry
    │      Message Type → Subscriber Callbacks
    │
    ├── Incoming Message Queue
    │
    ├── Dispatch Queue
    │
    └── Dispatcher
           dispatchAll()
```

The minimum initial API shall conceptually support:

```cpp
class DispatchPort
{
public:
    template<typename Message, typename Callback>
    SubscriptionHandle subscribe(Callback&& callback);

    template<typename Message>
    void send(Message&& message);

    void dispatchAll();
};
```

Exact signatures may change during implementation.

### Publishing

A model publishes using:

```cpp
port.send(message);
```

`send()` shall:

1. Determine the message type.
2. Safely store the message.
3. Add the message to the pending queue.
4. Return without invoking subscribers.

### Subscription

A model subscribes using:

```cpp
port.subscribe<MotorCommand>(
    [this](const MotorCommand& msg)
    {
        onMotorCommand(msg);
    });
```

The DispatchPort maintains the association:

```text
Message Type
     │
     ├── Subscriber A
     ├── Subscriber B
     └── Subscriber C
```

Multiple subscribers may subscribe to the same message type.

### Dispatch

`dispatchAll()` shall:

1. Establish the current dispatch batch.
2. Iterate through queued messages.
3. Determine each message's routing information.
4. Find matching subscriptions.
5. Invoke each applicable callback.
6. Destroy/release completed message storage.
7. Leave messages generated during callbacks pending for the next dispatch cycle.

### Subscription Lifetime

The design shall eventually provide explicit subscription lifetime management.

Preferred future usage:

```cpp
SubscriptionHandle subscription_;

subscription_ =
    port.subscribe<MotorCommand>(...);
```

Destroying or resetting the handle should safely unsubscribe the callback.

This prevents callbacks from referencing destroyed model instances.

---

# 6. Logging Schema

Logging shall initially support human-readable local console output.

A standard log record should contain:

```text
timestamp
severity
component
event
message
```

Example:

```text
12:00:01.015 INFO  ROS.DispatchPort  DISPATCH  MotorCommand subscribers=2
12:00:01.016 DEBUG MotorModel        RX        MotorCommand targetRpm=1500
12:00:01.017 DEBUG MotorModel        TX        MotorStatus currentRpm=1420
```

Recommended severity levels:

```text
TRACE
DEBUG
INFO
WARN
ERROR
FATAL
```

ROS Messaging should eventually expose diagnostic events including:

```text
MESSAGE_QUEUED
MESSAGE_DISPATCHED
MESSAGE_DROPPED
NO_SUBSCRIBERS
SUBSCRIBER_ADDED
SUBSCRIBER_REMOVED
QUEUE_FULL
CALLBACK_ERROR
```

Logging must not become a required dependency of model business logic.

A logging interface may therefore be injected or implemented as another framework service.

Embedded builds shall eventually allow logging to be compiled out or filtered by severity.

---

# 7. CLI Interface

A host executable shall provide a simple CLI for development and testing.

Initial usage may resemble:

```text
rtos_sim [options]
```

Recommended options:

```text
--help
--version
--frames <count>
--frame-rate <hz>
--log-level <level>
--list-models
--list-topics
```

Logging switches are process arguments: `rtos_sim --debug`, `rtos_sim --info`,
or `rtos_sim --noLogging`. Normal launches emit only `ERROR` and `FATAL`
records. In the interactive shell, `run` starts continuous execution on a
background worker so status and diagnostics remain available. `stop models`,
`stop sim`, or `quit` stops that worker. `run <frames>` remains available for a
fixed run.

Example:

```bash
rtos_sim --frames 1000 --frame-rate 100 --log-level debug
```

Expected behavior:

```text
RTOS Model Simulator

Frame Rate:   100 Hz
Frame Count:  1000
Models:       3
ROS Port:     main

Starting...

[000001] SensorModel update
[000001] ControlModel update
[000001] MotorModel update
[000001] ROS dispatch: 3 messages
```

The CLI is primarily intended as a development, simulation, debugging, and automated-testing interface.

---

# 8. File Structure

Recommended initial repository structure:

```text
cpp-rtos/
│
├── CMakeLists.txt
├── README.md
│
├── docs/
│   ├── architecture.md
│   ├── ros_messaging.md
│   └── model_specification.md
│
├── include/
│   └── rtos/
│       │
│       ├── model/
│       │   ├── BaseModel.hpp
│       │   └── ModelRunner.hpp
│       │
│       ├── messaging/
│       │   ├── DispatchPort.hpp
│       │   ├── Subscription.hpp
│       │   └── Message.hpp
│       │
│       ├── logging/
│       │   └── Logger.hpp
│       │
│       └── platform/
│           └── Platform.hpp
│
├── src/
│   ├── model/
│   ├── messaging/
│   ├── logging/
│   └── platform/
│
├── models/
│   ├── SensorModel/
│   ├── ControlModel/
│   └── MotorModel/
│
├── messages/
│   ├── MotorCommand.hpp
│   ├── MotorStatus.hpp
│   └── SensorData.hpp
│
├── platforms/
│   ├── host/
│   │   └── HostRunner.cpp
│   │
│   └── freertos/
│       └── FreeRTOSModelRunner.cpp
│
├── apps/
│   └── rtos_sim/
│       └── main.cpp
│
└── tests/
    ├── messaging/
    │   ├── DispatchPortTests.cpp
    │   └── SubscriptionTests.cpp
    │
    ├── models/
    └── integration/
```

The structure should remain modular. Agents should avoid introducing unnecessary abstractions merely to conform exactly to this proposed directory layout.

---

# 9. Milestones

### Milestone 1 — Project Foundation

Establish:

* C++20 project;
* CMake build;
* host executable;
* unit-test framework;
* basic logging;
* repository directory structure.

Acceptance criteria:

```text
Project builds locally.
Tests execute locally.
No cloud dependency is required.
Simple host application runs successfully.
```

### Milestone 2 — DispatchPort MVP

Implement:

```cpp
send<T>()
subscribe<T>()
dispatchAll()
```

Support:

* strongly typed messages;
* one publisher;
* one subscriber;
* multiple messages;
* deferred dispatch.

Acceptance criteria:

```text
send() does not invoke callbacks immediately.

dispatchAll() invokes the correct subscriber.

Messages preserve their data after the original
publisher's object goes out of scope.

Messages sent during dispatch are deferred until
the next dispatch cycle.
```

### Milestone 3 — Pub/Sub Routing

Add:

* multiple subscribers;
* multiple message types;
* subscription registry;
* routing tests;
* no-subscriber behavior.

Test:

```text
MotorCommand
    ├── MotorModel
    └── LoggerModel

SensorData
    └── ControlModel
```

Each message must reach only its registered subscribers.

### Milestone 4 — Model Framework

Implement:

```cpp
BaseModel
ModelRunner
initialize()
begin()
freeze()
operate()
terminate()
```

Create example:

```text
SensorModel
ControlModel
MotorModel
```

Models shall communicate exclusively through ROS Messaging for inter-model communication.

### Milestone 5 — Frame-Based Simulator

Implement the host execution cycle:

```cpp
while (running)
{
    modelRunner.update();
    rosPort.dispatchAll();
    waitForNextFrame();
}
```

Support configurable:

```text
frame count
frame rate
logging
```

Verify deterministic dispatch boundaries.

The host simulator shall run each enabled model in its own worker process. The
coordinator remains the source of truth for frame number and simulation time and
synchronizes those values to each worker before `operate()`. Every model worker
owns a separate `DispatchPort`; the coordinator routes transport envelopes
between those ports at the frame dispatch boundary.

Every model endpoint declares a transport type and routing ID. A message type
shall define `defaultRoutingId`, which `createPort<Message>()` uses when the
endpoint does not explicitly provide a routing ID.

### Milestone 6 — Subscription Lifetime

Implement safe subscription ownership.

Introduce:

```cpp
SubscriptionHandle
```

Ensure a destroyed model cannot leave behind a callback containing an invalid `this` pointer.

`SubscriptionHandle` shall be move-only and use RAII ownership. Resetting or
destroying the handle unregisters its callback. Unsubscription during an active
dispatch shall also invalidate a callback already copied into that dispatch
snapshot.

Add tests covering model creation/destruction and unsubscribe behavior.

### Milestone 7 — Concurrency

Introduce multiple C++ threads.

Example:

```text
Sensor Thread ─────┐
Control Thread ────┼──> DispatchPort
Network Thread ────┘
```

Make publishing thread-safe.

The host coordinator shall issue model `operate()` requests concurrently and
join them at an explicit barrier before routing. Delivery and dispatch may run
concurrently across model workers, but message order within one worker and the
frame dispatch boundary must remain deterministic. Dispatch queues,
subscriptions, traffic counters, IPC transactions, and callback lifetime state
shall be synchronized.

Study and document:

```text
mutexes
atomics
critical sections
race conditions
deadlocks
message ownership
```

### Milestone 8 — Bounded Messaging

Introduce RTOS-oriented constraints:

```text
fixed queue depth
maximum message size
queue-full behavior
drop policies
memory pools
static allocation
```

The framework should begin moving away from assumptions that unlimited heap allocation is available.

`DispatchPort` shall accept an immutable `QueueConfiguration` defining queue
depth, maximum payload size, and one of three full-queue policies: reject newest,
drop newest, or drop oldest. Publishing shall return a `SendResult`, and queue
statistics shall expose rejected, dropped, oversize, pending, and high-water
counts.

The incoming queue and dispatch batch shall reserve their complete capacity at
construction. Pending payloads shall use aligned inline storage so normal
publish and dispatch operations do not allocate payload objects.

### Milestone 9 — FreeRTOS Adapter

Implement the first real RTOS execution adapter.

Models remain unchanged.

Target architecture:

```text
FreeRTOS
   │
   ├── Task
   │     └── ModelRunner
   │            └── Model
   │
   └── Messaging Task
          └── DispatchPort
```

Demonstrate the same example models operating under both:

```text
Host Simulator

and

FreeRTOS
```

The portable adapter shall create one periodic task per registered model and a
separate messaging task. Model periods, priorities, and stack depths shall be
explicit. Its model registry shall be fixed-size, and the native FreeRTOS kernel
binding shall use statically allocated task control blocks and stacks through
`xTaskCreateStatic()`.

The native binding is optional at build time and shall link against an externally
provided `freertos_kernel` target. Application model source shall contain no
FreeRTOS APIs.

### Milestone 10 — Parallel Processing Exercise

Introduce models/tasks executing concurrently across available cores where supported.

The host implementation shall allow the operating system to schedule separate
model worker PIDs across available cores. The coordinator shall measure parallel
frame phases rather than execute model IPC requests sequentially.

Measure:

```text
execution time
dispatch latency
queue depth
deadline misses
jitter
CPU utilization
```

Use the exercise to explore real-time behavior rather than simply maximizing throughput.

---

# 10. Future Work

ROS Messaging may eventually support richer RTOS and distributed-system concepts.

Potential extensions include:

**Message Priorities**

```cpp
port.send<Priority::High>(EmergencyStop{});
```

**Queue Depth / QoS**

```text
Reliable
Best Effort
Keep Latest
Keep All
Drop Oldest
Drop Newest
```

**Topic Abstraction**

Allow multiple logical topics using the same C++ message type.

```cpp
port.subscribe<Temperature>(
    Topic{"engine.temperature"},
    callback);
```

**Multiple Dispatch Ports**

Example:

```text
controlPort
telemetryPort
diagnosticPort
networkPort
```

Models explicitly connect to the ports relevant to their responsibilities. In
the host simulator, each model worker owns its own IPC `DispatchPort`.

The initial host implementation assigns a sequential number to every named model
endpoint. Each endpoint declares its message type and publisher or subscriber
direction. Port diagnostics report the connected publisher and subscriber port
numbers, allowing the CLI `ports` command to display the live message topology.

Host model arguments are loaded from `xml/models.xml`. Each entry controls model
enablement, component DEBUG output, and whether the host launches an xterm GDB
attachment for that model. Every enabled model runs in a separate child PID and
owns a separate IPC dispatch port. Disabled models are not constructed,
scheduled, or represented by message ports. Missing model control-status reports
are presented as `STOPPED` by the host.

**Dedicated Messaging Tasks**

```text
Publishers
    │
    ▼
Concurrent Queue
    │
    ▼
ROS Messaging Task
    │
    ▼
Subscribers
```

**Cross-Core Messaging**

Allow models executing on different processor cores to communicate through bounded queues.

**Static Memory Pools**

Replace general-purpose dynamic allocation with deterministic fixed-capacity message pools.

**Message Metadata**

Add:

```text
timestamp
sequence number
source
destination
priority
deadline
correlation ID
```

**Performance Instrumentation**

Measure:

```text
send → dispatch latency
callback execution time
messages/frame
queue high-water mark
dropped messages
deadline violations
dispatch duration
```

**Watchdog Integration**

Detect:

```text
stalled models
missed frames
blocked messaging tasks
excessive callback execution
queue starvation
```

**Recording and Replay**

Record ROS Messaging traffic:

```text
timestamp | message type | payload
```

and replay it into models for deterministic debugging and regression testing.

**Transport Bridges**

Eventually bridge ROS Messaging to:

```text
UART
CAN
UDP
TCP
shared memory
actual ROS 2
```

without changing application models.

**Model Scheduling Metadata**

Models may eventually declare execution requirements:

```cpp
ModelConfig{
    .period = 10ms,
    .priority = Priority::High,
    .coreAffinity = 1
};
```

The RTOS integration layer can use this information to determine task scheduling.

---

# Core Design Principles

Agents implementing this specification shall preserve the following principles unless a deliberate architecture change is documented.

1. **Models communicate through messages, not direct model-to-model dependencies.**
2. **Models shall remain largely RTOS-independent.**
3. **ROS Messaging shall provide typed Publish/Subscribe communication.**
4. **`send()` queues; `dispatchAll()` dispatches.**
5. **Messages generated during dispatch are deferred to the next dispatch cycle.**
6. **Message ownership and subscription lifetime must always be explicit and safe.**
7. **RTOS-specific behavior belongs in adapters/execution layers rather than application models.**
8. **Host simulation and testing are first-class targets.**
9. **Determinism is preferred over cleverness.**
10. **The architecture should remain simple enough that its concurrency behavior can be understood, measured, and tested.**

The purpose of this project is not merely to construct a messaging library. It is to provide a practical environment for learning and exercising **modern C++, RTOS architecture, threads, synchronization, scheduling, parallel processing, deterministic message passing, and scalable model-based software design**.

