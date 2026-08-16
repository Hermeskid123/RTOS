# Architecture

The framework separates application models, ROS Messaging, execution, and
platform concerns. Milestone 1 establishes the host build and test foundation;
later milestones will add each subsystem without coupling models to an RTOS.

The `rtos_framework` CMake target contains portable framework code. Platform
executables and tests consume it through the `rtos::framework` alias target.
