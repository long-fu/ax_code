# ThreadPool + Bus async push

**Date:** 2026-08-20  
**Status:** Approved

## Goal

1. Generic `utils/thread_pool.h` for fire-and-forget / future tasks.
2. `BusProcess` owns a pool member; Alert/Visitor HTTP push runs off the Bus thread.

## ThreadPool

- Fixed worker count (default 2), bounded queue (default 128).
- `Submit(...)` → `std::optional<std::future<R>>`; queue full → `nullopt` (no block).
- Destructor / `Shutdown()`: stop accepting, drain workers, join.
- Does not replace `pipeline::TaskScheduler`.

## Bus wiring

- Member: `std::unique_ptr<ThreadPool> push_pool_` (created in `Init`).
- After building `AlertPushRequest` / `VisitorPushRequest` (owned copies of JPEG blobs), `Submit` lambda that calls `FaceServerClient::Push*`.
- On `nullopt` or push failure: log only; pipeline continues.

## Out of scope

- Priority queue, cancel tokens, moving ArcFace infer into the pool.
