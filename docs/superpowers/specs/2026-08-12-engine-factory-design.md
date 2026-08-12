# Engine Factory Design

**Date:** 2026-08-12  
**Status:** Approved — implement  
**Approach:** yaml-cpp (`third-party/yaml-cpp`) + `EngineFactory`

## Goal

Create an engine factory that reads a YAML config via yaml-cpp, uses `model_type` to select `Yolov5` or `Scrfd`, wires it into `InfProcess`, and ships sample configs.

## Non-goals

- Tracker / rules integration
- Auto-discovery of new model types beyond registered ones
- Hand-rolled YAML parser
## API

```cpp
namespace engine_factory {
  std::unique_ptr<Engine> CreateEngine(const std::string& config_path);
}
```

Returns `nullptr` on parse/unknown-type/IO failure (logged).

## YAML fields

| Field | Required | Used by |
|-------|----------|---------|
| `model_type` | yes | `yolov5` \| `scrfd` |
| `model_file` | yes | both |
| `inputs` | no | both |
| `prob_threshold` | no | both |
| `nms_threshold` | no | both |
| `strides` | no | both |
| `num_anchors` | no | both |
| `anchors` | no | yolov5 |
| `labels` | no | both |

## Files

| Path | Change |
|------|--------|
| `nodes/engine_factory.h` | add |
| `nodes/engine_factory.cpp` | add (yaml-cpp LoadFile + CreateEngine) |
| `nodes/inf_process.h` | store path; Init creates engine |
| `apps/main_rtsp.cpp` | pass sample config path |
| `configs/yolov5.yaml` | add |
| `configs/scrfd.yaml` | add |
| `CMakeLists.txt` | include/link `third-party/yaml-cpp`; `--allow-shlib-undefined` for cross-link |

## InfProcess

- Ctor keeps `model_config_path_`
- `Init()`: `engine_ = engine_factory::CreateEngine(...)`; fail if null
- Hot path unchanged (`engine_->Process` / `Postprocess`)

## Manual test

User switches `configs/yolov5.yaml` vs `configs/scrfd.yaml` in `main_rtsp` and runs on device.
