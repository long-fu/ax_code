#pragma once

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "detection_types.h"
#include "engine.h"
#include "image_data.h"
#include "ivps_helper.h"

struct ArcfaceConfig : public EngineConfig {
  std::string config_path;
  std::string model_file = "model/w600k_r50-bgr-std.axmodel.onnx";
  std::string model_type = "arcface";
  std::vector<int> inputs = {1, 3, 112, 112};
  int feat_dim = 512;
  bool l2_normalize = false;
  float expand_ratio = 1.5f;

  std::string ModelFile() const override { return model_file; }
};

class Arcface : public Engine {
 public:
  explicit Arcface(const ArcfaceConfig& config);
  ~Arcface() override;

  // Detection API unused for recognition; returns 0.
  int Postprocess(int pic_width, int pic_height,
                  std::vector<detection::Object>& objects) override;

  // Call after Process(). Fills feat with feat_dim floats (optionally L2).
  int Extract(std::vector<float>& feat);

  // Align face (IVPS ROI + NormCrop) and pack bytes for Process().
  int Preprocess(IvpsHelper& ivps, const ImageData& frame,
                 const detection::Object& face, ImageData &face_img, std::vector<uint8_t>& out);

  // CPU NormCrop from full-frame BGR Mat + full-image landmarks, then pack.
  // int Preprocess(const cv::Mat& bgr, const detection::Object& face,
  //                std::vector<uint8_t>& out);

  // Preprocess + Process + Extract.
  int Infer(IvpsHelper& ivps, const ImageData& frame,
            const detection::Object& face,ImageData &face_img, std::vector<float>& feat);

  // Run Infer for each Scrfd face. feats.size() == faces.size();
  // failed faces leave feats[i] empty. Returns number of successes.
  int InferBatch(IvpsHelper& ivps, const ImageData& frame,
                 const std::vector<detection::Object>& faces,
                 std::vector<ImageData>& face_imgs,
                 std::vector<std::vector<float>>& feats);

  Arcface(const Arcface&) = delete;
  Arcface& operator=(const Arcface&) = delete;

 private:
  int PackAlignedFace(const cv::Mat& aligned_bgr, std::vector<uint8_t>& out);
  int InputHeight() const;
  int InputWidth() const;

  ArcfaceConfig config_;
};
