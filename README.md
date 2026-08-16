# C++ RTOS Model Framework

A host-testable C++20 foundation for exploring RTOS models, scheduling, and the
project's ROS Messaging publish/subscribe architecture. The framework includes
the model lifecycle, status-reporting runner, frame-based host simulator, shared
simulation clock, example models, and strongly typed deferred publish/subscribe
routing.

Subscriptions use move-only RAII handles. Destroying or resetting a
`SubscriptionHandle` safely unregisters its callback, including callbacks that
capture a model's `this` pointer.

Messaging queues have configurable fixed depth, maximum payload size, and
full-queue policy. Queue payload storage is reserved at construction and does
not allocate while publishing or dispatching.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler
- A POSIX host with `fork()` and pipes for multi-process simulation

No network access or third-party downloads are required.

## Build

```bash
./build
```

The build command configures a Debug build in `.build` and compiles it in
parallel. Pass `--test` to compile and execute the unit-test cases:

```bash
./build --test
./build --test -j40
./build clean
./build clobber
./build -j40
```

Set `BUILD_DIR` or `BUILD_TYPE` to override their defaults. Additional arguments
are forwarded to the CMake configure command. `scripts/build.sh` remains as a
compatibility wrapper around `./build`.

## Run

```bash
./.build/rtos_sim
./.build/rtos_sim --help
./run --debug
./run --frames 100 --frame-rate 50
./run --frames 1000 --frame-rate 100 --metrics
```

The simulator starts one coordinator process and a separate worker PID with its
own IPC dispatch port for every enabled model. The interactive shell supports:

```text
start
status
ports
messages
stop models
stop sim
quit
```

The interactive `run` command starts continuous execution on a background
worker, leaving the prompt available for `status`, `ports`, `messages`, `stop
models`, and `stop sim`. Use `run <frames>` for an interactive fixed run, or
launch with `--frames <count>` for a fixed run that exits automatically. The
`--frame-rate <hz>` option controls frame pacing and defaults to 100 Hz.
Use the interactive `metrics` command, or add `--metrics` to a fixed run, to show
parallel execution time, dispatch latency, queue depth, jitter, deadline misses,
and worker CPU utilization. See [`docs/concurrency.md`](docs/concurrency.md).

Logging is configured when launching the executable. `./run` prints `ERROR` and
`FATAL` records by default, `./run --info` and `./run --debug` increase
verbosity, and `./run --noLogging` suppresses every log record.

Model arguments live in `xml/models.xml`. Each model can be enabled or disabled,
allowed to emit DEBUG records, and marked for GDB attachment. Use the interactive
`models` command to inspect the loaded configuration or launch with
`./run --models <file>` to select another XML file. See
[`docs/model_configuration.md`](docs/model_configuration.md).

Host binaries include GDB symbols by default. On Linux, an enabled model with
`debug="true"` or `gdb="true"` enables runtime `ptrace` attachment for the
simulator process so the configured xterm GDB session can attach without Yama
permission failures.

The run script uses the default build directory, builds the project if needed,
and forwards optional simulator arguments:

```bash
./run
./run --info
./scripts/run.sh --help
```

## Test

```bash
ctest --test-dir .build --output-on-failure
```

Tests live under `tests/unit/` and use a small repository-local harness registered
with CTest, keeping the project dependency-free and suitable for offline
development. The standard test command is:

```bash
./build --test
```

## Layout

- `include/rtos/` — public framework headers
- `src/` — framework implementation
- `apps/rtos_sim/` — host simulator entry point
- `models/` — application model implementations
- `messages/` — strongly typed message definitions
- `platforms/` — host and RTOS adapters
- `tests/` — unit and integration tests
- `docs/` — architecture and subsystem documentation

## ROS Messaging

`rtos::messaging::DispatchPort` accepts strongly typed messages through `send()`
and routes them to callbacks registered through `subscribe<Message>()` when
`dispatchAll()` is called. The current host implementation supports multiple
message types and multiple subscribers while preserving a deterministic dispatch
boundary. Per-type subscriber counts and per-cycle dispatch reports make routing
and no-subscriber behavior observable.

Models create explicitly named and numbered message endpoints such as
`MotorCommand_port`. The interactive `ports` command lists each endpoint and the
port numbers publishing to or subscribing from it.

Concrete project messages are defined in `messages/`. See
[`docs/ros_messaging.md`](docs/ros_messaging.md) for API semantics and examples.
See [`docs/bounded_messaging.md`](docs/bounded_messaging.md) for bounded queue
configuration and [`docs/freertos_adapter.md`](docs/freertos_adapter.md) for the
optional native FreeRTOS execution target.
