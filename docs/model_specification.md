# Model Specification

This lifecycle and dependency contract is stable for release 1.0.0.

Models are independently executable application components with
framework-managed lifecycle functions. Every model derives from
`rtos::model::BaseModel` and implements the following lifecycle:

1. `initialize()` prepares dependencies and subscriptions before execution.
2. `begin()` starts or resumes execution.
3. `operate()` performs one scheduled model cycle.
4. `freeze()` pauses execution without tearing the model down.
5. `terminate()` performs final cleanup.

The execution environment owns lifecycle scheduling. Models remain independent
of concrete RTOS APIs and communicate with one another only through ROS
Messaging.

Example models receive both `DispatchPort` and `Logger` through their
constructors. Lifecycle changes and ROS Messaging transmit/receive activity are
logged at `DEBUG` level with `INITIALIZE`, `BEGIN`, `FREEZE`, `TERMINATE`, `TX`,
and `RX` event names.

Every lifecycle operation returns a `ControlStatus`. Initialization and freeze
return `STOPPED`, begin and active operation return `RUNNING`, and teardown
returns `TERMINATED`. `ModelRunner` aggregates these results so host and RTOS
execution environments can report every model's current state.

`BaseModel::currentFrame()` and `BaseModel::clockTime()` expose the shared frame
counter and monotonic simulation clock mirrored into the model worker by the host
coordinator. The host simulation core is the only authoritative owner allowed to
advance frames or control time, keeping all process-local model views consistent
at each dispatch boundary.

Models that subscribe with a callback capturing `this` retain the returned
move-only `SubscriptionHandle` as a member. Termination may reset it explicitly;
normal model destruction also unregisters it automatically, preventing the
dispatch registry from retaining a callback to a destroyed model.

Models may execute in a host worker PID or a FreeRTOS task without source
changes. Their constructors receive messaging and logging dependencies; frame
advancement, scheduling, task priority, process creation, and transport routing
remain execution-layer responsibilities.
