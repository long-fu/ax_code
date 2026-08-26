#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "arcface.h"
#include "detection_types.h"
#include "drawing.h"
#include "face_server_client.h"
#include "image_data.h"
#include "ivps_helper.h"
#include "qdrant_client.hpp"
#include "thread_pool.h"

class HostServices {
public:
    HostServices();
    ~HostServices();

    HostServices(const HostServices&) = delete;
    HostServices& operator=(const HostServices&) = delete;

    int Init();
    void Shutdown();

    int InferFaces(const ImageData& frame,
                   const std::vector<detection::Object>& faces,
                   std::vector<ImageData>& face_imgs,
                   std::vector<std::vector<float>>& feats);

    int CloneFrame(ImageData& dest, const ImageData& src);
    int EncodeJpeg(std::vector<uint8_t>& dest, const ImageData& src);
    bool CropFaceRoi(const ImageData& frame, const detection::Object& face,
                     ImageData& face_img);
    bool EncodeVisitorFace(const ImageData& face_img,
                           std::vector<uint8_t>& face_jpg);

    void DrawRect(const ImageData& frame, int x1, int y1, int x2, int y2,
                  const YUVColor& color, int line_width);
    void DrawText(const ImageData& frame, int x, int y, const std::string& text,
                  const YUVColor& color, int font_size);
    void DrawCircle(const ImageData& frame, int cx, int cy, int radius,
                    const YUVColor& color);

    // 提交到宿主线程池。plugin_name 用于 per-plugin 在途计数。
    // 队列满返回 false，不入队。
    bool SubmitAsync(const std::string& plugin_name, std::function<void()> fn);
    // timeout_ms < 0 表示无限等待。返回是否在超时前归零。
    bool WaitQuiesce(const std::string& plugin_name, int timeout_ms = -1);

    qdrant::QdrantClient* Vectors();
    face_server::FaceServerClient* Notify();
    bool VectorsReady() const;
    bool NotifyReady() const;

    float ConfigFloat(const char* key, float def) const;
    std::string ConfigString(const char* key, const std::string& def) const;

private:
    struct InflightGate {
        std::atomic<int> count{0};
        std::mutex mu;
        std::condition_variable cv;
    };

    std::shared_ptr<InflightGate> GetOrCreateGate(const std::string& name);

    IvpsHelper* ivps_ = nullptr;
    IvpsHelper* push_ivps_ = nullptr;
    std::mutex push_ivps_mutex_;
    std::unique_ptr<Arcface> engine_;
    std::unique_ptr<qdrant::QdrantClient> client_;
    std::unique_ptr<face_server::FaceServerClient> face_server_;
    std::unique_ptr<my_utils::ThreadPool> push_pool_;

    std::unordered_map<std::string, std::string> config_kv_;
    std::mutex gates_mutex_;
    std::unordered_map<std::string, std::shared_ptr<InflightGate>> gates_;
    bool inited_ = false;
};
