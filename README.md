# C++ RTOS Model Framework

A host-testable C++20 foundation for exploring RTOS models, scheduling, and the
project's ROS Messaging publish/subscribe architecture. The framework currently
implements Milestone 1 from [`Project.md`](Project.md).

## Requirements

- CMake 3.20 or newer
- A C++20 compiler

No network access or third-party downloads are required.

## Build

```bash
./build
```

The build command configures a Debug build in `.build`, compiles it in parallel,
and runs all tests. It also supports cleanup and explicit parallelism:

```bash
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

Tests use a small repository-local harness registered with CTest, keeping the
foundation dependency-free and suitable for offline development.

## Layout

- `include/rtos/` — public framework headers
- `src/` — framework implementation
- `apps/rtos_sim/` — host simulator entry point
- `models/` — application model implementations
- `messages/` — strongly typed message definitions
- `platforms/` — host and RTOS adapters
- `tests/` — unit and integration tests
- `docs/` — architecture and subsystem documentation
