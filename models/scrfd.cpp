#include "scrfd.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "detection.h"
#include "logger.h"

namespace
{

    struct ScrfdLevel
    {
        int stride = 0;
        const float* score = nullptr;
        const float* bbox = nullptr;
        const float* kps = nullptr;
        size_t n = 0; // H*W*2
    };

    // InsightFace packed layout: row = spatial_index * 2 + anchor.
    // Convert to CHW expected by GenerateProposalsScrfd:
    //   score[q * feat + i], bbox[(q*4+c)*feat + i], kps[(q*10+c)*feat + i]
    void PackedToChw(const float* score_n1, const float* bbox_n4,
                     const float* kps_n10, size_t n, int feat_size,
                     std::vector<float>& score_chw, std::vector<float>& bbox_chw,
                     std::vector<float>& kps_chw)
    {
        constexpr int kNumAnchors = 2;
        score_chw.assign(static_cast<size_t>(kNumAnchors) * feat_size, 0.f);
        bbox_chw.assign(static_cast<size_t>(kNumAnchors) * 4 * feat_size, 0.f);
        kps_chw.assign(static_cast<size_t>(kNumAnchors) * 10 * feat_size, 0.f);

        for (int q = 0; q < kNumAnchors; ++q)
        {
            for (int index = 0; index < feat_size; ++index)
            {
                const size_t row = static_cast<size_t>(index) * kNumAnchors + q;
                if (row >= n)
                {
                    continue;
                }
                score_chw[static_cast<size_t>(q) * feat_size + index] = score_n1[row];
                for (int c = 0; c < 4; ++c)
                {
                    bbox_chw[static_cast<size_t>(q * 4 + c) * feat_size + index] = bbox_n4[row * 4 + c];
                }
                for (int c = 0; c < 10; ++c)
                {
                    kps_chw[static_cast<size_t>(q * 10 + c) * feat_size + index] = kps_n10[row * 10 + c];
                }
            }
        }
    }

    int StrideFromN(size_t n, int letterbox)
    {
        if (n == 0 || n % 2 != 0)
        {
            return 0;
        }
        const size_t hw = n / 2;
        for (int stride : {8, 16, 32})
        {
            const int feat = letterbox / stride;
            if (static_cast<size_t>(feat) * static_cast<size_t>(feat) == hw)
            {
                return stride;
            }
        }
        return 0;
    }

} // namespace

Scrfd::~Scrfd() = default;

int Scrfd::Postprocess(int pic_width, int pic_height,
                       std::vector<detection::Object>& objects)
{
    objects.clear();

    const int lb_h = 640;
    const int lb_w = 640;

    for (int i = 0; i < GetOutput().nOutputSize; ++i)
    {
        auto out = GetOutput().pOutputs[i];
        auto name = GetInfo()->pOutputs[i].pName;

        const size_t floats = out.nSize / sizeof(float);
        // LOG_INFO("Scrfd Postprocess: output {} name={} size={} floats={}", i, name, out.nSize, floats);
        if (out.pVirAddr == nullptr || out.nSize == 0)
        {
            LOG_ERROR("Scrfd Postprocess: empty output {}", i);
            return -1;
        }
    }

// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 0 name=448 size=51200 floats=12800
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 1 name=471 size=12800 floats=3200
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 2 name=494 size=3200 floats=800
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 3 name=451 size=204800 floats=51200
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 4 name=474 size=51200 floats=12800
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 5 name=497 size=12800 floats=3200
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 6 name=454 size=512000 floats=128000
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 7 name=477 size=128000 floats=32000
// [08-18 11:36:15.242] [info] [tid:901968] Scrfd Postprocess: output 8 name=500 size=32000 floats=8000


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

    detection::generate_proposals_scrfd_my(
        8, ptr_448, ptr_451, ptr_454,
        config_.prob_threshold, proposals, lb_w, lb_h);

    detection::generate_proposals_scrfd_my(
        16, ptr_471, ptr_474, ptr_477,
        config_.prob_threshold, proposals, lb_w, lb_h);

    detection::generate_proposals_scrfd_my(
        32, ptr_494, ptr_497, ptr_500,
        config_.prob_threshold, proposals, lb_w, lb_h);

    // LOG_INFO("Det10g Postprocess: {} proposals before NMS pic {}x{}", proposals.size(), pic_height, pic_width);
    detection::get_out_bbox(proposals, objects, config_.nms_threshold, lb_h, lb_w,
                            pic_height, pic_width);
    // LOG_INFO("out box size={}", objects.size());
    return 0;
}