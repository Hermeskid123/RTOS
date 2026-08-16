# Concurrency And Parallel Processing

The host simulator combines threads and processes while retaining one
deterministic dispatch boundary per frame.

## Frame Barriers

Each frame has three parallel phases:

1. The coordinator launches one asynchronous request per model worker. Each
   worker executes `operate()` in its own PID and may be scheduled on a different
   CPU core.
2. After every model completes, the coordinator routes the completed outbound
   batch. Delivery runs concurrently across workers while preserving message
   order within each worker.
3. After delivery completes, every worker dispatches concurrently. The next
   frame cannot start until all dispatch requests complete.

This operate → route → dispatch barrier prevents model completion order from
changing which messages belong to a frame. Messages published by callbacks stay
deferred until the next frame.

## Synchronization

- `DispatchPort` uses a mutex for its incoming queue, traffic counters, topology,
  dispatch state, and queue high-water mark. Publishing only holds this mutex
  while updating shared state; transport sends and subscriber callbacks execute
  outside it.
- `SubscriptionRegistry` uses a separate mutex for registration, removal, count,
  and callback-snapshot creation. Callbacks execute after releasing the registry
  mutex.
- Each subscription slot uses an atomic active flag plus an in-flight count and
  condition variable. Unsubscribe prevents new invocation and waits for a
  callback already executing on another thread. A callback may unsubscribe
  itself without waiting on itself.
- `ModelProcess` serializes each pipe request/response transaction with a mutex.
  Different model processes remain independent and can execute concurrently.
- The IPC worker's buffered transport uses a mutex so concurrent publications
  cannot corrupt its outbound batch.

Callbacks never run while a dispatch-port or registry container mutex is held.
This lock separation avoids recursive-send and self-unsubscribe deadlocks.
Objects owning a handle must still synchronize concurrent access to the handle
object itself; the registry and dispatch port provide the cross-thread safety.

## Message Ownership

Every publication owns a copied or moved message payload before returning.
Dispatch swaps the current incoming queue into a private batch, allowing
publishers to fill the next batch concurrently. Subscription snapshots retain
shared callback slots, while inactive slots are skipped safely.

## Performance Metrics

The `metrics` command reports frame and model execution time, send-to-dispatch
latency, callback and dispatch-phase time, messages per frame, queue high-water
mark, scheduling jitter, deadline misses, and normalized worker CPU utilization.

Worker CPU utilization uses process CPU time divided by measured frame wall time
and available hardware threads. It is an exercise-level estimate rather than an
OS profiler replacement. Thread creation, pipe traffic, and coordinator routing
are included in frame execution time. Use `--frames <count> --metrics` for a
repeatable fixed run.
