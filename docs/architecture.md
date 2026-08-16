# Architecture

The framework separates application models, ROS Messaging, execution, and
platform concerns. Milestone 1 establishes the host build and test foundation.
Milestones 2 and 3 add deferred typed messaging and Pub/Sub routing without
coupling models to an RTOS.

The `rtos_framework` CMake target contains portable framework code. Platform
executables and tests consume it through the `rtos::framework` alias target.

Within ROS Messaging, `DispatchPort` owns pending message batches and delegates
type-to-callback routing to `SubscriptionRegistry`. `DispatchReport` exposes the
result of each completed batch without requiring logging.
