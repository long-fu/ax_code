# Final lifecycle fix report

## Scope and source

- Start HEAD: `4d58501295b52f39370bebbc8f3956e1df6f51a0`.
- Commit message: `fix(plugin): harden dynamic lifecycle cleanup`.
- Final commit and tree hashes are reported by the caller after commit; a
  commit cannot contain its own hash.
- Scope is limited to the two final whole-branch lifecycle findings. No ABI
  version, dependency, secret, sequence logic, architecture, or production
  dynamic-library path was added.

## Fixes

- `PluginManager` validates `BusinessPlugin::Name()` immediately after factory
  creation and before API checks or `Init`. Null, empty, and non-matching names
  fail startup. Diagnostics include configured scene, library, `name` stage,
  and a safe actual-name rendering (`<null>`/`<empty>` or the returned name).
- The configured name is the canonical business name. Successful unload and
  failed-Init cleanup both use it for `WaitQuiesce`, while the ABI contract
  requires plugins to use the validated `Name()` for `SubmitAsync`.
- After business `Init` is invoked, every failure executes
  `WaitQuiesce -> Shutdown -> DestroyPlugin -> dlclose`.
- After postprocessor `Init` is invoked, every failure executes
  `Shutdown -> DestroyPostProcessor -> dlclose`.
- Previously initialized postprocessors continue to roll back in reverse order.
- Public ABI comments document host guarantees and partial-Init/idempotent-
  Shutdown obligations. The stale business comment now describes one plugin
  consuming read-only postprocessed objects and externally supplied `track_id`.

## Deterministic test evidence

- Added real fake shared libraries for empty/mismatched business names,
  business failed partial Init, and postprocessor failed partial Init.
- Name tests prove rejection occurs before `Init`, followed by destroy/dlclose.
- The business partial-Init fake allocates state and submits an async task.
  Test synchronization uses condition variables plus a 2000 ms failure bound:
  it proves Shutdown has not occurred while the task is blocked, then releases
  the task and checks the exact wait/shutdown/destroy/dlclose order. No sleeps.
- The postprocessor fake records allocated state and proves Shutdown sees and
  clears it before destroy/dlclose.
- SceneRuntime rollback expectation now includes Shutdown for failed business
  Init while retaining reverse processor rollback.

## Commands and results

- Target cross-build:
  `cmake --build build --target business_plugin_loader_test postprocessor_chain_test scene_runtime_test -j2`
  — PASS, all three AArch64 targets built.
- Clean full build: temporary uncommitted `third-party` symlink to
  `/home/haoshuai/code/ax_core/third-party`, then `./build.sh`
  — PASS, default `all` reached `[100%] Built target ax_core`.
- Artifact check: `ax_core`, `libax_runtime.so`, face plugin, tracker plugin,
  and lifecycle test binary are all ARM AArch64 ELF files.
- ByteTrack ownership: zero matching symbols in `ax_core` and face plugin;
  45 matching `BYTETracker|STrack::` symbols in `libtracker.so`.
- RPATH: face plugin begins with `$ORIGIN/..`; tracker is exactly `$ORIGIN/..`.
- Required config/plugin artifacts exist.
- `git diff --check` — PASS.
- Temporary `third-party` symlink removed and `test ! -e third-party` — PASS.

## Runtime status and residual risk

- Host execution of the lifecycle binaries: NOT RUN. The build is AArch64,
  this host is x86_64, and `qemu-aarch64` is unavailable. This is the exact
  blocker; compile/link and deterministic test code were verified, but runtime
  assertions require the ARM target.
- ARM CTest and business smoke: NOT RUN — user manual, as required.
- Existing compiler warnings in legacy model/ByteTrack/HAL code remain; no new
  warning was observed from the lifecycle changes.
