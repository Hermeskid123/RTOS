# Architecture

Release 1.0.0 separates application models, ROS Messaging, execution, and
platform concerns. The completed milestone baseline is recorded in
[`../Release.md`](../Release.md).

`rtos::simulation::SimulatorCore` is the coordinator's simulation singleton and
source of truth for frame number and elapsed time. Each model runs in a separate
worker PID. Before `operate()`, the coordinator sends the authoritative frame and
time over IPC so the worker's process-local core mirrors them. `BaseModel`
retains read-only references to that local view and cannot advance simulator
state.

The `rtos_framework` CMake target contains portable framework code. Platform
executables and tests consume it through the `rtos::framework` alias target.

Within ROS Messaging, `DispatchPort` owns pending message batches and delegates
type-to-callback routing to `SubscriptionRegistry`. `DispatchReport` exposes the
result of each completed batch without requiring logging.

Each model worker owns a distinct `DispatchPort` configured with the `IPC`
transport. Published trivially-copyable messages become transport envelopes
containing a message name, routing ID, and payload. The coordinator broadcasts
each frame's completed outbound batch to matching subscriber routes, then asks
every worker to dispatch. Callback publications remain deferred until the next
boundary.

Within each frame, coordinator threads issue `operate()` requests to every model
PID concurrently, join at the routing barrier, then issue dispatch requests
concurrently. See [`concurrency.md`](concurrency.md) for synchronization, lock
ordering, ownership, and performance instrumentation details.

`DispatchPort` queue capacity, payload size, and overflow behavior are explicit.
Incoming and dispatch batches use preallocated inline payload slots rather than
allocating a shared payload for each publication. See
[`bounded_messaging.md`](bounded_messaging.md).

The execution layer also provides a portable `FreeRtosAdapter`. Its native
binding creates static FreeRTOS model tasks plus a dedicated messaging task,
while models retain the same lifecycle and messaging APIs used by the host
simulator. See [`freertos_adapter.md`](freertos_adapter.md).

## Dependency Direction

Application models depend on framework interfaces, never other models or
platform kernels. The host process adapter and native FreeRTOS binding depend on
the framework. Platform-specific code does not flow back into model headers.
