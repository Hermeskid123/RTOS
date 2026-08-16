# Architecture

The framework separates application models, ROS Messaging, execution, and
platform concerns. Milestone 1 establishes the host build and test foundation.
Milestones 2 and 3 add deferred typed messaging and Pub/Sub routing without
coupling models to an RTOS.

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
