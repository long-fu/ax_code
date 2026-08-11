# Pipeline Interface Migration Plan

> **For agentic workers:** Fix all broken references after pipeline API rename.

**Goal:** Update all callers of old Pipeline/PipelineThread API to use new pipeline::TaskScheduler/pipeline::TaskNode API.

**Architecture:** Mechanical rename mapping old→new. Apply sed + manual edits to 6 files.

| Old | New |
|-----|-----|
| `#include "pipeline.h"` | `#include "task_scheduler.h"` |
| `#include "pipeline_thread.h"` | `#include "task_node.h"` |
| `#include "pipeline_resource.h"` | `#include "resource.h"` |
| `PipelineThread` | `pipeline::TaskNode` |
| `Pipeline` | `pipeline::TaskScheduler` |
| `PipelineResource` | `pipeline::Resource` |
| `PipelineThreadParam` | `pipeline::TaskNodeParam` |
| `SelfInstanceId()` | `InstanceId()` |
| `SelfInstanceName()` | `InstanceName()` |
| `GetPipelineThreadIdByName` | `TaskNodeIdByName` |
| `CreatePipelineInstance` | `CreateTaskSchedulerInstance` |
| `GetPipelineInstance` | `GetTaskSchedulerInstance` |
| `WaitEnd()` | `SignalWaitEnd()` |
| `g_main_thread_id` | `0` |
| `thread_inst` | `node` |
| `thread_inst_name` | `node_name` |
| `thread_inst_id` | `node_id` |
| `SendMessage(` | `pipeline::SendMessage(` (when as free function) |

## Global Constraints

- Build: `rm -rf build && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -S . -B build && cmake --build build`
- CMakeLists.txt must list new pipeline source files
- Add `using namespace pipeline;` where appropriate, or use explicit `pipeline::` prefix
