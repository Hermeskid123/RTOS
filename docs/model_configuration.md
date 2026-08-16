# Model Configuration

Release 1.0.0 uses this XML schema for host model selection and debugging.

The host simulator loads model arguments from `xml/models.xml` by default.
Choose another file with `./run --models <file>`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<models>
    <model name="SensorModel" enabled="true" debug="true" gdb="false" />
    <model name="ControlModel" enabled="true" debug="true" gdb="false" />
    <model name="MotorModel" enabled="true" debug="true" gdb="false" />
</models>
```

- `enabled` controls whether the simulator constructs and schedules the model.
- `debug` permits DEBUG records from that model when the process is launched
  with `--debug`.
- `gdb` launches `xterm -e gdb -p <pid>` for that model's dedicated worker PID
  when the simulator starts. The xterm title identifies the requested model.

Host builds include GDB symbols by default through
`RTOS_ENABLE_DEBUG_SYMBOLS=ON`. On Linux, any enabled model with `debug="true"`
or `gdb="true"` enables debugger attachment. Linux workers call
`prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY)`, allowing the xterm GDB process to
attach under the normal Yama `ptrace_scope=1` policy.

The interactive `models` command displays the loaded values. Disabled models do
not create message ports and are not added to `ModelRunner`.

The interactive `status` command treats a model with no returned control-status
report as `STOPPED`.

The parser requires a `<models>` root, at least one `<model>`, unique non-empty
model names, and boolean values expressed as `true`, `false`, `1`, or `0`.
Invalid files fail startup with a descriptive exception.
