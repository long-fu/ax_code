#include "arcface.h"

#include <cmath>
#include <cstring>

#include "face_align.h"
#include "logger.h"

namespace {

bool BgrToNv12(const cv::Mat& bgr, std::vector<uint8_t>& out) {
  if (bgr.empty() || bgr.type() != CV_8UC3) {
    return false;
  }
  const int w = bgr.cols;
  const int h = bgr.rows;
  if ((w & 1) || (h & 1)) {
    return false;
  }

  cv::Mat i420;
  cv::cvtColor(bgr, i420, cv::COLOR_BGR2YUV_I420);
  out.resize(static_cast<size_t>(w) * h * 3 / 2);

  std::memcpy(out.data(), i420.data, static_cast<size_t>(w) * h);
  const uint8_t* u = i420.data + w * h;
  const uint8_t* v = u + (w * h / 4);
  uint8_t* uv = out.data() + w * h;
  for (int i = 0; i < w * h / 4; ++i) {
    uv[i * 2] = u[i];
    uv[i * 2 + 1] = v[i];
  }
  return true;
}

bool BgrToRgbPacked(const cv::Mat& bgr, std::vector<uint8_t>& out) {
  if (bgr.empty() || bgr.type() != CV_8UC3) {
    return false;
  }
  cv::Mat rgb;
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  if (!rgb.isContinuous()) {
    rgb = rgb.clone();
  }
  out.assign(rgb.data, rgb.data + static_cast<size_t>(rgb.total()) * rgb.elemSize());
  return true;
}

}  // namespace

Arcface::Arcface(const ArcfaceConfig& config)
    : Engine(config), config_(config) {}

Arcface::~Arcface() = default;

int Arcface::InputHeight() const {
  if (config_.inputs.size() >= 4) {
    return config_.inputs[2];
  }
  return 112;
}

int Arcface::InputWidth() const {
  if (config_.inputs.size() >= 4) {
    return config_.inputs[3];
  }
  return 112;
}

int Arcface::Postprocess(int /*pic_width*/, int /*pic_height*/,
                         std::vector<detection::Object>& objects) {
  objects.clear();
  return 0;
}

int Arcface::Extract(std::vector<float>& feat) {
  feat.clear();

  auto* info = GetInfo();
  if (info == nullptr || info->nOutputSize == 0) {
    LOG_ERROR("Arcface::Extract: no output info");
    return -1;
  }

  auto& out = GetOutput().pOutputs[0];
  const auto* ptr = static_cast<const float*>(out.pVirAddr);
  if (ptr == nullptr || out.nSize == 0) {
    LOG_ERROR("Arcface::Extract: empty output buffer");
    return -2;
  }

  const size_t floats = out.nSize / sizeof(float);
  const size_t dim = static_cast<size_t>(config_.feat_dim);
  if (floats < dim) {
    LOG_ERROR("Arcface::Extract: output floats {} < feat_dim {}", floats, dim);
    return -3;
  }

  feat.assign(ptr, ptr + dim);

  if (config_.l2_normalize) {
    double sum_sq = 0.0;
    for (float v : feat) {
      sum_sq += static_cast<double>(v) * static_cast<double>(v);
    }
    const double norm = std::sqrt(sum_sq);
    if (norm > 1e-12) {
      const float inv = static_cast<float>(1.0 / norm);
      for (float& v : feat) {
        v *= inv;
      }
    }
  }

  return 0;
}

int Arcface::PackAlignedFace(const cv::Mat& aligned_bgr,
                             std::vector<uint8_t>& out) {
  out.clear();
  if (aligned_bgr.empty() || aligned_bgr.type() != CV_8UC3) {
    LOG_ERROR("Arcface::PackAlignedFace: need CV_8UC3 aligned face");
    return -1;
  }

  const int expect_h = InputHeight();
  const int expect_w = InputWidth();
  cv::Mat face = aligned_bgr;
  if (face.rows != expect_h || face.cols != expect_w) {
    cv::resize(face, face, cv::Size(expect_w, expect_h), 0, 0,
               cv::INTER_LINEAR);
  }

  auto* info = GetInfo();
  if (info == nullptr || info->nInputSize == 0) {
    LOG_ERROR("Arcface::PackAlignedFace: engine not inited / no input info");
    return -2;
  }

  const size_t expect_bytes = info->pInputs[0].nSize;
  const size_t nv12_bytes =
      static_cast<size_t>(expect_w) * expect_h * 3 / 2;
  const size_t rgb_bytes = static_cast<size_t>(expect_w) * expect_h * 3;

  if (expect_bytes == nv12_bytes) {
    if (!BgrToNv12(face, out)) {
      LOG_ERROR("Arcface::PackAlignedFace: BGR->NV12 failed");
      return -3;
    }
  } else if (expect_bytes == rgb_bytes) {
    if (!BgrToRgbPacked(face, out)) {
      LOG_ERROR("Arcface::PackAlignedFace: BGR->RGB failed");
      return -4;
    }
  } else {
    LOG_ERROR(
        "Arcface::PackAlignedFace: unsupported input size {} "
        "(nv12={}, rgb={})",
        expect_bytes, nv12_bytes, rgb_bytes);
    return -5;
  }

  if (out.size() != expect_bytes) {
    LOG_ERROR("Arcface::PackAlignedFace: packed {} != expect {}", out.size(),
              expect_bytes);
    return -6;
  }
  return 0;
}

int Arcface::Preprocess(IvpsHelper& ivps, const ImageData& frame,
                        const detection::Object& face,
                        std::vector<uint8_t>& out) {
  const int image_size = InputWidth();
  cv::Mat aligned = face_align::HwRoiNormCrop(ivps, frame, face,
                                              config_.expand_ratio, image_size);
  if (aligned.empty()) {
    LOG_ERROR("Arcface::Preprocess: HwRoiNormCrop failed");
    return -1;
  }
  return PackAlignedFace(aligned, out);
}

// int Arcface::Preprocess(const cv::Mat& bgr, const detection::Object& face,
//                         std::vector<uint8_t>& out) {
//   const int image_size = InputWidth();
//   cv::Mat aligned = face_align::NormCrop(bgr, face, image_size);
//   if (aligned.empty()) {
//     LOG_ERROR("Arcface::Preprocess: NormCrop failed");
//     return -1;
//   }
//   return PackAlignedFace(aligned, out);
// }

int Arcface::Infer(IvpsHelper& ivps, const ImageData& frame,
                   const detection::Object& face, std::vector<float>& feat) {
  std::vector<uint8_t> input;
  int ret = Preprocess(ivps, frame, face, input);
  if (ret != 0) {
    return ret;
  }
  ret = Process(input);
  if (ret != 0) {
    LOG_ERROR("Arcface::Infer: Process failed, ret={}", ret);
    return ret;
  }
  return Extract(feat);
}

int Arcface::InferBatch(IvpsHelper& ivps, const ImageData& frame,
                        const std::vector<detection::Object>& faces,
                        std::vector<std::vector<float>>& feats) {
  feats.clear();
  feats.resize(faces.size());

  int ok = 0;
  for (size_t i = 0; i < faces.size(); ++i) {
    const int ret = Infer(ivps, frame, faces[i], feats[i]);
    if (ret == 0) {
      ++ok;
    } else {
      feats[i].clear();
      LOG_ERROR("Arcface::InferBatch: face[{}] failed, ret={}", i, ret);
    }
  }
  return ok;
}
