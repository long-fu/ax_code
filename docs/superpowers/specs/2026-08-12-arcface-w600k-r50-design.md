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

**Call sequence:**
- Low-level: `Init()` → `Process(aligned_bytes)` → `Extract(feat)`
- Integrated: `Init()` → `Infer(ivps, frame, face, feat)`  
  (= `HwRoiNormCrop` / `NormCrop` → pack NV12 or RGB by model input size → `Process` → `Extract`)

## Preprocess integration

```cpp
int Preprocess(IvpsHelper&, const ImageData&, const detection::Object&,
               std::vector<uint8_t>& out);  // HW ROI + NormCrop + pack
int Preprocess(const cv::Mat& bgr, const detection::Object&,
               std::vector<uint8_t>& out);   // CPU NormCrop + pack
int Infer(...);  // Preprocess + Process + Extract
```

Packing auto-selects by `GetInfo()->pInputs[0].nSize`:
- `112*112*3/2` → NV12
- `112*112*3` → RGB packed HWC

## Factory

- Sample: `configs/arcface.yaml`
- `engine_factory`: `model_type: arcface` → `std::make_unique<Arcface>(cfg)`
- Do **not** wire into `InfProcess` / RTSP detection path in this change

## Non-goals (updated)

- Gallery search / BusProcess integration
- Changing `Engine` base virtuals
- 106-point landmark model (`2d106det`) — this util uses **5-point** from detector (e.g. Scrfd)

## Face align utility (added)

Static helpers for InsightFace ArcFace 5-point crop → `image_size×image_size` (default 112).

**File:** `models/face_align.h` (+ `.cpp` if needed; prefer header-inline only if small)

```cpp
namespace face_align {

// InsightFace arcface_dst for 112 (left-eye, right-eye, nose, left-mouth, right-mouth)
// Scaled when image_size != 112: dst = arcface_dst * (image_size/112.f)

// Estimate 2x3 similarity (partial affine) from 5 landmarks → template.
cv::Mat EstimateNorm(const cv::Point2f landmark[5], int image_size = 112);

// Warp BGR/RGB/gray Mat to aligned square crop.
cv::Mat NormCrop(const cv::Mat& image,
                 const cv::Point2f landmark[5],
                 int image_size = 112);

// Overload from detection::Object landmarks.
cv::Mat NormCrop(const cv::Mat& image,
                 const detection::Object& face,
                 int image_size = 112);

// Hardware ROI crop (IVPS) then CPU NormCrop — preferred on-device path.
cv::Rect ComputeExpandedRoi(const cv::Rect_<float>& box, float expand_ratio,
                            int frame_w, int frame_h);
void RemapLandmarksToRoi(const cv::Point2f src[5], cv::Point2f dst[5],
                         float roi_x, float roi_y);
cv::Mat HwRoiNormCrop(IvpsHelper& ivps, const ImageData& frame,
                      const detection::Object& face,
                      float expand_ratio = 1.5f, int image_size = 112);

}  // namespace face_align
```

**Option-1 pipeline (hardware crop + CPU align):**

```mermaid
flowchart LR
  det[Scrfd_bbox_landmark]
  hw[IVPS_CropAndCSC_RGB888]
  map[Remap_landmarks_to_ROI]
  align[NormCrop_112]
  det --> hw --> map --> align
```

1. Expand `face.rect` by `expand_ratio` (default 1.5), then grow to at least `image_size×image_size` when the frame allows (small faces), clamp + even-align for IVPS  
2. `IvpsHelper::CropAndCSC(AX_FORMAT_RGB888, …)` + `Process`  
3. `Copy2Mat` → remapped landmarks → `NormCrop` → always **112×112** (warpAffine upscales if ROI &lt; 112 and frame itself was too small)  

**Impl notes:**
- Umeyama 2D similarity + `cv::warpAffine` (no `opencv_calib3d` dependency), `borderValue=0`
- Full-image landmarks for plain `NormCrop`; ROI-local landmarks after `HwRoiNormCrop` remap
- Does **not** convert to NV12 / Engine input packing — caller converts for `Arcface::Process`
- IVPS does **not** perform landmark affine; only bbox crop + CSC

## Files (updated)

| Path | Action |
|---|---|
| `models/arcface.h` / `.cpp` | add |
| `models/face_align.h` / `.cpp` | add (+ HwRoiNormCrop) |
| `models/engine_factory.cpp` | register `arcface` |
| `configs/arcface.yaml` | add |
| `docs/.../2026-08-12-arcface-w600k-r50-design.md` | this update |

## Typical usage

```cpp
// CPU-only (full-frame Mat + full-image landmarks)
cv::Mat aligned = face_align::NormCrop(bgr, obj.landmark, 112);

// On-device: IVPS ROI crop then NormCrop
cv::Mat aligned = face_align::HwRoiNormCrop(ivps, frame, obj, 1.5f, 112);

// ... pack aligned to model input bytes ...
arcface.Process(bytes);
arcface.Extract(feat);
```
