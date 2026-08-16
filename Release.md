# Release 1.0.0

## Agent Export Summary — Creating Models

Copy this section into another agent's context when asking it to add a model.

```text
Project: C++20 RTOS model framework, release 1.0.0.

Architecture rule:
- Models never reference other models; all inter-model communication uses typed
  messages through rtos::messaging::DispatchPort.

Model contract:
- Create models/<ModelName>/<ModelName>.hpp and .cpp.
- Derive the class from rtos::model::BaseModel.
- Constructor dependencies are DispatchPort& and Logger&.
- Implement initialize(), begin(), freeze(), operate(), terminate(), and status().
- Return STOPPED after initialize/freeze, RUNNING after begin/active operate, and
  TERMINATED after terminate.
- Use currentFrame() and clockTime() for coordinator-owned simulation state.

Messaging contract:
- Put message structs in messages/*.hpp.
- Messages must be small, trivially copyable data-only structs.
- Each message defines static constexpr string_view name and a unique static
  constexpr uint32_t defaultRoutingId.
- Create typed publisher/subscriber endpoints with createPort<Message>(name,
  PortDirection::publisher/subscriber).
- send() only queues; callbacks run later at dispatchAll().
- Store every SubscriptionHandle as a model member when its callback captures
  this. Reset it during terminate(); RAII destruction is the final safeguard.
- Callback-generated messages are delivered at the next dispatch boundary.

Host integration checklist:
1. Add the model .cpp to rtos_framework in CMakeLists.txt.
2. Add the model name to xml/models.xml.
3. Include and construct it in createModel() in apps/rtos_sim/ModelProcess.cpp.
4. Describe its ports in topologyFor() in that same file.
5. Add lifecycle and routing tests under tests/unit/ and register the source in
   tests/CMakeLists.txt.
6. Run ./build --test; all existing and new tests must pass.

Reference implementations:
- models/SensorModel: publisher-only model.
- models/ControlModel: subscriber plus publisher model.
- models/MotorModel: subscriber plus publisher with safe subscription lifetime.
- messages/SensorData.hpp and messages/MotorCommand.hpp: message definitions.

Do not put FreeRTOS calls, process APIs, scheduling, direct model dependencies,
global dispatch ports, or owning references to temporary message data in models.
```

Release 1.0.0 completes the original ten project milestones and establishes the
first stable host-simulator, ROS Messaging, concurrency, bounded-storage, and
FreeRTOS-adapter baseline.

## Release Status

- **Version:** 1.0.0
- **Language:** C++20
- **Host platform:** POSIX process and pipe support
- **Embedded platform:** Optional FreeRTOS kernel binding
- **External services:** None
- **Validation:** 49 host tests passing

## Completed Milestones

### Milestone 1 — Project Foundation

Completed with a CMake-based C++20 library, host executable, local test harness,
structured logger, build/run scripts, and modular source tree. The project builds
and tests without network or cloud dependencies.

Evidence: `CMakeLists.txt`, `apps/rtos_sim/main.cpp`, `tests/TestFramework.hpp`,
and `include/rtos/logging/Logger.hpp`.

### Milestone 2 — DispatchPort MVP

Completed with strongly typed `send<T>()`, `subscribe<T>()`, and `dispatchAll()`.
Publications own their payload, callbacks are deferred, ordering is preserved,
and callback-generated messages wait for the next dispatch boundary.

Evidence: `include/rtos/messaging/DispatchPort.hpp` and
`tests/unit/messaging/DispatchPortTests.cpp`.

### Milestone 3 — Pub/Sub Routing

Completed with exact-type routing, multiple subscribers, multiple message types,
subscription counts, no-subscriber handling, named ports, routing identifiers,
and topology/traffic diagnostics.

Evidence: `include/rtos/messaging/SubscriptionRegistry.hpp`,
`include/rtos/messaging/PortTopology.hpp`, and messaging unit tests.

### Milestone 4 — Model Framework

Completed with `BaseModel`, `ModelRunner`, `ControlStatus`, and the complete
initialize/begin/freeze/operate/terminate lifecycle. `SensorModel`,
`ControlModel`, and `MotorModel` communicate exclusively through messages.

Evidence: `include/rtos/model/`, `models/`, and
`tests/unit/model/ModelFrameworkTests.cpp`.

### Milestone 5 — Frame-Based Simulator

Completed with configurable frame count/rate/logging, a singleton simulator core,
authoritative frame counter and clock, one worker PID and dispatch port per model,
IPC transport envelopes, message routing IDs, and deterministic frame dispatch
boundaries.

Evidence: `include/rtos/simulation/`, `apps/rtos_sim/`, and
`tests/integration/ModelProcessTests.cpp`.

### Milestone 6 — Subscription Lifetime

Completed with a move-only RAII `SubscriptionHandle`. Reset/destruction removes
callbacks safely, active cross-thread callbacks are awaited, self-unsubscribe is
deadlock-safe, and destroyed models cannot leave callbacks capturing invalid
`this` pointers.

Evidence: `include/rtos/messaging/SubscriptionHandle.hpp` and subscription/model
lifetime tests.

### Milestone 7 — Concurrency

Completed with concurrent publishers, synchronized dispatch state and traffic,
thread-safe subscription registration/removal, serialized IPC transactions, and
explicit operate/route/dispatch barriers. Callbacks execute without container
mutexes held.

Evidence: `docs/concurrency.md`, messaging concurrency tests, and the host
coordinator implementation.

### Milestone 8 — Bounded Messaging

Completed with immutable queue depth and maximum payload size, reject-newest,
drop-newest, and drop-oldest policies, explicit send results, queue statistics,
preallocated incoming/dispatch buffers, and aligned inline payload storage.

Evidence: `include/rtos/messaging/QueueConfiguration.hpp`,
`docs/bounded_messaging.md`, and bounded-queue tests.

### Milestone 9 — FreeRTOS Adapter

Completed with a portable fixed-capacity task registry, one periodic task per
model, a dedicated messaging task, explicit stack/priority/period configuration,
and an optional native binding using `xTaskCreateStatic()`. Application models
remain FreeRTOS-independent and the existing example models are shown in the
native integration guide.

Evidence: `include/rtos/platform/freertos/`, `platforms/freertos/`,
`docs/freertos_adapter.md`, and FreeRTOS adapter tests.

### Milestone 10 — Parallel Processing Exercise

Completed with parallel worker-process operate, delivery, and dispatch phases;
deterministic barriers; and measurement of execution time, dispatch latency,
queue depth, deadline misses, jitter, callback time, and worker CPU utilization.

Evidence: `include/rtos/simulation/PerformanceMetrics.hpp`,
`docs/concurrency.md`, and performance-metrics tests.

## Compatibility And Limits

- Messages transported through `DispatchPort` must be trivially copyable and no
  larger than the configured bounded payload size.
- Host model workers require POSIX `fork()`, pipes, and process APIs.
- The native FreeRTOS target requires an externally supplied `freertos_kernel`
  CMake target and the configuration documented in `docs/freertos_adapter.md`.
- ROS Messaging is project-internal and is not ROS 1 or ROS 2 compatible.

## Validation Commands

```bash
./build --test
ctest --test-dir .build --output-on-failure
./run --frames 1000 --frame-rate 100 --metrics
```

## Documentation

- `Project.md` — active architecture and product specification
- `README.md` — build, run, and user entry point
- `docs/architecture.md` — subsystem architecture
- `docs/ros_messaging.md` — messaging API and dispatch semantics
- `docs/model_specification.md` — model lifecycle contract
- `docs/model_configuration.md` — host XML configuration
- `docs/concurrency.md` — synchronization and parallel execution
- `docs/bounded_messaging.md` — bounded queue behavior
- `docs/freertos_adapter.md` — native FreeRTOS integration
