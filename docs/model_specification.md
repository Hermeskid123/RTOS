# Model Specification

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
