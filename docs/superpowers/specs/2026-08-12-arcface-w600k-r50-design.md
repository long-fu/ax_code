# Arcface (w600k_r50) Feature Extractor Design

**Date:** 2026-08-12  
**Status:** Approved in conversation (Engine subclass + name `Arcface`)

## Goal

Wrap InsightFace buffalo_l `w600k_r50` recognition network as `Arcface : public Engine`, matching existing Yolov5/Scrfd packaging. Deployed model: `model/w600k_r50.axmodel` (ONNX reference: `model/buffalo_l/w600k_r50.onnx`).

## Network I/O

| | Shape / notes |
|---|---|
| Input | `1×3×112×112` (NV12/RGB prep owned by caller) |
| Output | `1×512` embedding (float) |
| Post | Optional L2 normalize before compare |

## API

```cpp
struct ArcfaceConfig : public EngineConfig {
  std::string model_file = "model/w600k_r50.axmodel";
  std::string model_type = "arcface";
  std::vector<int> inputs = {1, 3, 112, 112};
  int feat_dim = 512;
  bool l2_normalize = true;
  std::string ModelFile() const override { return model_file; }
};

class Arcface : public Engine {
 public:
  explicit Arcface(const ArcfaceConfig& config);
  int Postprocess(int, int, std::vector<detection::Object>&) override;  // no-op, return 0
  int Extract(std::vector<float>& feat);  // after Process(); L2 if configured
};
```

**Call sequence:** `Init()` → `Process(aligned_112x112_bytes)` → `Extract(feat)`.

## Factory

- Sample: `configs/arcface.yaml`
- `engine_factory`: `model_type: arcface` → `std::make_unique<Arcface>(cfg)`
- Do **not** wire into `InfProcess` / RTSP detection path in this change

## Non-goals

- Face align / landmark (`2d106det`)
- Gallery search / BusProcess integration
- Changing `Engine` base virtuals (keeps Yolov5/Scrfd untouched)
- Completing Chinese OSD fonts or unrelated HAL work

## Files

| Path | Action |
|---|---|
| `models/arcface.h` | add |
| `models/arcface.cpp` | add |
| `models/engine_factory.cpp` | register `arcface` |
| `configs/arcface.yaml` | add |

## Verification

- Cross-build `ax_core` with new sources
- Manual: load yaml / construct `Arcface`, `Init` with on-device axmodel (device test by user)
