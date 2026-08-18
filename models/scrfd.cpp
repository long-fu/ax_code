#include "scrfd.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "detection.h"
#include "detection_types.h"
#include "logger.h"
#include "opencv2/core/types.hpp"

namespace
{
    constexpr int kFmc = 3;
    constexpr int kNumAnchors = 2;
    constexpr int kStrides[3] = {8, 16, 32};

    struct ScrfdLevel
    {
        int stride = 0;
        const float* score = nullptr;
        const float* bbox = nullptr;
        const float* kps = nullptr;
        size_t n = 0; // H*W*2
    };

    float iou(const detection::Object& a, const detection::Object& b)
    {
        cv::Rect intersection = a.rect & b.rect;

        if (intersection.empty())
            return 0.0;

        double intersection_area = intersection.area();
        double union_area = a.rect.area() + b.rect.area() - intersection_area;

        return intersection_area / union_area;
    }

    std::vector<int> nms(const std::vector<detection::Object>& dets, float thresh)
    {
        std::vector<int> order(dets.size());
        for (size_t i = 0; i < dets.size(); ++i)
            order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return dets[a].prob > dets[b].prob; });
        std::vector<int> keep;
        std::vector<char> suppressed(dets.size(), 0);
        for (size_t _i = 0; _i < order.size(); ++_i)
        {
            int i = order[_i];
            if (suppressed[i])
                continue;
            keep.push_back(i);
            for (size_t _j = _i + 1; _j < order.size(); ++_j)
            {
                int j = order[_j];
                if (suppressed[j])
                    continue;
                if (iou(dets[i], dets[j]) >= thresh)
                    suppressed[j] = 1;
            }
        }
        return keep;
    }
} // namespace

Scrfd::~Scrfd() = default;

int Scrfd::Postprocess(int pic_width, int pic_height,
                       std::vector<detection::Object>& objects)
{
    objects.clear();

    // 1. 修正输入宽高索引 (inputs[2]=H, inputs[3]=W)
    const int input_h = config_.inputs[2];
    const int input_w = config_.inputs[3];

    const float im_ratio = static_cast<float>(pic_height) / static_cast<float>(pic_width);
    const float model_ratio = static_cast<float>(input_h) / static_cast<float>(input_w);
    int new_w = 0, new_h = 0;
    if (im_ratio > model_ratio)
    {
        new_h = input_h;
        new_w = static_cast<int>(static_cast<float>(new_h) / im_ratio);
    }
    else
    {
        new_w = input_w;
        new_h = static_cast<int>(static_cast<float>(new_w) * im_ratio);
    }
    if (new_w < 1)
        new_w = 1;
    if (new_h < 1)
        new_h = 1;
    const float det_scale = static_cast<float>(new_h) / static_cast<float>(pic_height);

    std::vector<detection::Object> proposals;
    auto outs = GetOutput().pOutputs;
    for (int idx = 0; idx < kFmc; ++idx)
    {
        const int stride = kStrides[idx];
        float* scores = static_cast<float*>(outs[idx].pVirAddr);
        float* bboxes = static_cast<float*>(outs[idx + kFmc].pVirAddr);
        float* kps = static_cast<float*>(outs[idx + kFmc * 2].pVirAddr);

        const size_t scoreCount = outs[idx].nSize / sizeof(float);

        fprintf(stdout, "[%d] %d %ld %d %d \n", idx, stride, scoreCount, config_.inputs[2], config_.inputs[3]);

        const int feat_h = config_.inputs[2] / stride;
        const int feat_w = config_.inputs[3] / stride;

        CenterKey key{feat_h, feat_w, stride};
        auto it = centerCache_.find(key);
        if (it == centerCache_.end())
        {
            std::vector<float> centers;
            centers.reserve(static_cast<size_t>(feat_h * feat_w * kNumAnchors * 2));
            for (int ay = 0; ay < feat_h; ++ay)
            {
                for (int ax = 0; ax < feat_w; ++ax)
                {
                    const float cx = static_cast<float>(ax * stride);
                    const float cy = static_cast<float>(ay * stride);
                    for (int a = 0; a < kNumAnchors; ++a)
                    {
                        centers.push_back(cx);
                        centers.push_back(cy);
                    }
                }
            }
            it = centerCache_.emplace(key, std::move(centers)).first;
        }

        const auto& centers = it->second;

        const size_t nAnchors = centers.size() / 2;
        if (scoreCount < nAnchors)
            continue;

        for (size_t i = 0; i < nAnchors; ++i)
        {
            const float score = scores[i];
            if (score < config_.prob_threshold)
                continue;
            const float cx = centers[i * 2];
            const float cy = centers[i * 2 + 1];
            const float l = bboxes[i * 4 + 0] * stride;
            const float t = bboxes[i * 4 + 1] * stride;
            const float r = bboxes[i * 4 + 2] * stride;
            const float b = bboxes[i * 4 + 3] * stride;

            detection::Object box;
            box.prob = score;
            auto x1 = (cx - l) / det_scale;
            auto y1 = (cy - t) / det_scale;
            auto x2 = (cx + r) / det_scale;
            auto y2 = (cy + b) / det_scale;

            box.rect = cv::Rect(x1, y1, x2 - x1, y2 - y1);

            for (int k = 0; k < 5; ++k)
            {
                const float px = cx + kps[i * 10 + k * 2] * stride;
                const float py = cy + kps[i * 10 + k * 2 + 1] * stride;
                box.landmark[k].x = px / det_scale;
                box.landmark[k].y = py / det_scale;
            }
            proposals.push_back(box);
        }
    }
    if (proposals.empty())
        return {};

    auto keep = nms(proposals, config_.nms_threshold);
    // std::vector<FaceBox> out;
    objects.reserve(keep.size());
    for (int i : keep)
        objects.push_back(proposals[i]);
    return 0;
}
