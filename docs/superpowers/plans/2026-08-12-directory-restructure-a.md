# Directory Restructure Plan (Option A)

> **For agentic workers:** Execute task-by-task. Checkbox tracking.

**Goal:** Split `nodes/` into `stages/` (pipeline stages) and `models/` (Engine + detectors + factory); keep `hal/ax` as device wrappers only.

**Architecture:** stages = Pre/Inf/Bus/Enc; models = Engine/Yolov5/Scrfd/factory/detection; hal = media + AX SDK helpers.

## Global Constraints

- Preserve RTSP build
- Rename InfProccess → InfProcess (and node_name string)
- Prefer `git mv` for history
- Update CMake include + aux_source paths

---

### Task 1: Create dirs + move stages
### Task 2: Move models + Engine + detection
### Task 3: Update CMake + includes + apps
### Task 4: Build verify
