# C++ RTOS Model Framework

A host-testable C++20 foundation for exploring RTOS models, scheduling, and the
project's ROS Messaging publish/subscribe architecture. The framework currently
implements Milestones 1–3 from [`Project.md`](Project.md), including strongly
typed deferred publish/subscribe routing.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler

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
```

The run script uses the default build directory, builds the project if needed,
and forwards optional simulator arguments:

```bash
./scripts/run.sh
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

Concrete project messages are defined in `messages/`. See
[`docs/ros_messaging.md`](docs/ros_messaging.md) for API semantics and examples.
