# FreeRTOS Adapter

This is the optional native execution adapter shipped in release 1.0.0.

The FreeRTOS adapter keeps application models independent of the kernel. Models
continue to derive from `BaseModel`, use their existing lifecycle methods, and
publish through `DispatchPort`.

`FreeRtosAdapter` creates:

- one periodic FreeRTOS task for every registered model;
- one periodic `ROSDispatch` task for `DispatchPort::dispatchAll()`;
- a fixed-size registry supporting up to 16 model tasks without heap allocation.

Each model task performs `initialize()`, `begin()`, periodic `operate()`, and
`terminate()`. Periods and priorities are independently configured. The
messaging task provides the explicit dispatch boundary.

## Native Binding

`NativeFreeRtosKernel` maps the portable adapter API to `xTaskCreateStatic()`,
`xTaskGetTickCount()`, `xTaskDelayUntil()`, and `vTaskDelete()`. Task control
blocks and stacks are statically owned by the binding. The FreeRTOS application
must enable static allocation and provide a CMake target named
`freertos_kernel` before adding this project:

`FreeRTOSConfig.h` must enable `configSUPPORT_STATIC_ALLOCATION`,
`INCLUDE_vTaskDelete`, and `INCLUDE_xTaskDelayUntil`.

```cmake
add_subdirectory(path/to/FreeRTOS-Kernel freertos-kernel)
set(RTOS_BUILD_FREERTOS_ADAPTER ON CACHE BOOL "" FORCE)
add_subdirectory(path/to/RTOS rtos-framework)

target_link_libraries(firmware PRIVATE rtos::freertos_adapter)
```

The number of static tasks and words per stack are configured with
`RTOS_FREERTOS_MAX_TASKS` and `RTOS_FREERTOS_MAX_STACK_DEPTH`. The default 17
task slots accommodate 16 models and the messaging task.

## Example Models

The existing example classes are used without FreeRTOS includes or source
changes:

```cpp
rtos::messaging::DispatchPort port{
    "freertos",
    rtos::messaging::QueueConfiguration{32, 64}
};
rtos::platform::freertos::NativeFreeRtosKernel kernel;
rtos::platform::freertos::FreeRtosAdapter adapter{kernel, port};

rtos::models::SensorModel sensor{port, logger};
rtos::models::ControlModel control{port, logger};
rtos::models::MotorModel motor{port, logger};

adapter.addModel("Sensor", sensor, {1024, 2, pdMS_TO_TICKS(10)});
adapter.addModel("Control", control, {1024, 3, pdMS_TO_TICKS(10)});
adapter.addModel("Motor", motor, {1024, 2, pdMS_TO_TICKS(10)});
adapter.start();
vTaskStartScheduler();
```

The adapter and its registered models must have static or application-lifetime
storage. `stop()` requests orderly task termination; the owning firmware is
responsible for coordinating shutdown before destroying those objects.
An adapter is intentionally single-use: models cannot be added and tasks cannot
be restarted after `start()` has been called.

The host test suite validates adapter task creation and lifecycle behavior with a
kernel test double. A firmware build validates the native binding against the
selected FreeRTOS port and its `FreeRTOSConfig.h`.
