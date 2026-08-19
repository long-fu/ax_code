#include "scrfd.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "detection.h"
#include "detection_types.h"
#include "logger.h"
#include "opencv2/core/operations.hpp"
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
    LOG_INFO("Postprocess");
    #if 0
    const int lb_h = config_.inputs[2];
    const int lb_w = config_.inputs[3];

    std::vector<detection::Object> proposals;

    auto& out_448 = GetOutput().pOutputs[0];
    const auto* ptr_448 = static_cast<const float*>(out_448.pVirAddr);

    auto& out_471 = GetOutput().pOutputs[1];
    const auto* ptr_471 = static_cast<const float*>(out_471.pVirAddr);

    auto& out_494 = GetOutput().pOutputs[2];
    const auto* ptr_494 = static_cast<const float*>(out_494.pVirAddr);

    auto& out_451 = GetOutput().pOutputs[3];
    const auto* ptr_451 = static_cast<const float*>(out_451.pVirAddr);

    auto& out_474 = GetOutput().pOutputs[4];
    const auto* ptr_474 = static_cast<const float*>(out_474.pVirAddr);

    auto& out_497 = GetOutput().pOutputs[5];
    const auto* ptr_497 = static_cast<const float*>(out_497.pVirAddr);

    auto& out_454 = GetOutput().pOutputs[6];
    const auto* ptr_454 = static_cast<const float*>(out_454.pVirAddr);

    auto& out_477 = GetOutput().pOutputs[7];
    const auto* ptr_477 = static_cast<const float*>(out_477.pVirAddr);

    auto& out_500 = GetOutput().pOutputs[8];
    const auto* ptr_500 = static_cast<const float*>(out_500.pVirAddr);

    detection::generate_proposals_scrfd(
        8, ptr_448, ptr_451, ptr_454,
        config_.prob_threshold, proposals, lb_w, lb_h);

    detection::generate_proposals_scrfd(
        16, ptr_471, ptr_474, ptr_477,
        config_.prob_threshold, proposals, lb_w, lb_h);

    detection::generate_proposals_scrfd(
        32, ptr_494, ptr_497, ptr_500,
        config_.prob_threshold, proposals, lb_w, lb_h);

    // LOG_INFO("Det10g Postprocess: {} proposals before NMS pic {}x{}", proposals.size(), pic_height, pic_width);
    detection::get_out_bbox(proposals, objects, config_.nms_threshold, lb_h, lb_w,
                            pic_height, pic_width);
#else

    // 1. 修正输入宽高索引 (inputs[2]=H, inputs[3]=W)
    const int input_h = config_.inputs[2];
    const int input_w = config_.inputs[3];

    // 2. 正确计算 Letterbox 的 Scale 和 Padding 偏移量
    const float scale_w = static_cast<float>(input_w) / static_cast<float>(pic_width);
    const float scale_h = static_cast<float>(input_h) / static_cast<float>(pic_height);
    const float det_scale = std::min(scale_w, scale_h); // 保持等比例缩放

    // 计算图像在 model input 中的实际占用尺寸与 Padding 偏移
    const float resized_w = pic_width * det_scale;
    const float resized_h = pic_height * det_scale;
    const float pad_w = (input_w - resized_w) * 0.5f; // 左右 padding 宽度
    const float pad_h = (input_h - resized_h) * 0.5f; // 上下 padding 高度

    std::vector<detection::Object> proposals;
    auto outs = GetOutput().pOutputs;
    for (int idx = 0; idx < kFmc; ++idx)
    {
        const int stride = kStrides[idx];
        float* scores = static_cast<float*>(outs[idx].pVirAddr);
        float* bboxes = static_cast<float*>(outs[idx + kFmc].pVirAddr);
        float* kps = static_cast<float*>(outs[idx + kFmc * 2].pVirAddr);

        const size_t scoreCount = outs[idx].nSize / sizeof(float);

        // fprintf(stdout, "[%d] %d %ld %d %d \n", idx, stride, scoreCount, config_.inputs[2], config_.inputs[3]);

        const int feat_h = input_h / stride;
        const int feat_w = input_w / stride;

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
                    // 修正：加上 +0.5f 像素中心偏移
                    const float cx = (static_cast<float>(ax) + 0.5f) * stride;
                    const float cy = (static_cast<float>(ay) + 0.5f) * stride;
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
        {
            LOG_ERROR("数据错误");
            continue;
        }

        for (size_t i = 0; i < nAnchors; ++i)
        {
            const float score = scores[i];
            // cv::print()
            // printf("score: %f\n", score);
            if (score < config_.prob_threshold)
                continue;
            const float cx = centers[i * 2];
            const float cy = centers[i * 2 + 1];
            const float l = bboxes[i * 4 + 0] * stride;
            const float t = bboxes[i * 4 + 1] * stride;
            const float r = bboxes[i * 4 + 2] * stride;
            const float b = bboxes[i * 4 + 3] * stride;

            // 解算模型 input 坐标系下的框
            float x1_raw = cx - l;
            float y1_raw = cy - t;
            float x2_raw = cx + r;
            float y2_raw = cy + b;

            // 修正：扣除 Padding 后再除以 scale 还原回原始图片坐标
            float x1 = (x1_raw - pad_w) / det_scale;
            float y1 = (y1_raw - pad_h) / det_scale;
            float x2 = (x2_raw - pad_w) / det_scale;
            float y2 = (y2_raw - pad_h) / det_scale;

            detection::Object box;
            box.label = 0;
            box.prob = score;
            box.rect = cv::Rect(x1, y1, x2 - x1, y2 - y1);

            // 修正：关键点同步扣除 Padding 并还原
            if (kps != nullptr)
            {
                for (int k = 0; k < 5; ++k)
                {
                    const float px_raw = cx + kps[i * 10 + k * 2 + 0] * stride;
                    const float py_raw = cy + kps[i * 10 + k * 2 + 1] * stride;
                    box.landmark[k].x = (px_raw - pad_w) / det_scale;
                    box.landmark[k].y = (py_raw - pad_h) / det_scale;
                }
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
    #endif
    return 0;
}
