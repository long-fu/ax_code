#include "host_services.h"

#include <chrono>
#include <cstdlib>
#include <utility>

#include "ax_global_type.h"
#include "face_align.h"
#include "logger.h"

HostServices::HostServices()
{
    ivps_ = new IvpsHelper(1920 * 1080 * 3, 32);
    push_ivps_ = new IvpsHelper(1920 * 1080 * 3, 32);
}

HostServices::~HostServices()
{
    Shutdown();
    delete ivps_;
    ivps_ = nullptr;
    delete push_ivps_;
    push_ivps_ = nullptr;
}

int HostServices::Init()
{
    if (inited_)
    {
        return 0;
    }

    ArcfaceConfig arc_cfg;
    engine_ = std::make_unique<Arcface>(arc_cfg);
    if (engine_ == nullptr || engine_->Init() != 0)
    {
        LOG_ERROR("HostServices: ArcFace Init failed");
        engine_.reset();
        return -1;
    }

    qdrant::QdrantConfig qcfg;
    qcfg.host = "192.168.137.112";
    qcfg.port = 6333;
    client_ = std::make_unique<qdrant::QdrantClient>(qcfg);
    if (!client_->Healthy())
    {
        LOG_ERROR("无法连接到 Qdrant,请检查服务是否启动");
        return 1;
    }

    face_server::FaceServerConfig fs_cfg;
    fs_cfg.base_url = "http://192.168.137.112:8848";
    if (const char* k = std::getenv("ALERTS_PUSH_API_KEY"))
    {
        fs_cfg.alerts_api_key = k;
    }
    if (const char* k = std::getenv("VISITORS_PUSH_API_KEY"))
    {
        fs_cfg.visitors_api_key = k;
    }
    else if (const char* k = std::getenv("FACE_SERVER_VISITORS_API_KEY"))
    {
        fs_cfg.visitors_api_key = k;
    }
    if (fs_cfg.alerts_api_key.empty() || fs_cfg.visitors_api_key.empty())
    {
        LOG_ERROR("FaceServer API Key 未配置: 设置 ALERTS_PUSH_API_KEY / VISITORS_PUSH_API_KEY");
    }
    face_server_ = std::make_unique<face_server::FaceServerClient>(std::move(fs_cfg));
    push_pool_ = std::make_unique<my_utils::ThreadPool>(2, 128);

    if (0 != ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, AX_FORMAT_RGB888, 112, 112))
    {
        LOG_ERROR("IVPS Init failed!");
        return -2;
    }
    if (0 != push_ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, AX_FORMAT_RGB888, 112,
                                112))
    {
        LOG_ERROR("push IVPS Init failed!");
        return -2;
    }

    constexpr float kDefaultFrontalScoreThresh = 0.55f;
    config_kv_["frontal_score_thresh"] = std::to_string(kDefaultFrontalScoreThresh);
    if (const char* thresh_env = std::getenv("AX_FRONTAL_SCORE_THRESH"))
    {
        try
        {
            (void)std::stof(thresh_env);
            config_kv_["frontal_score_thresh"] = thresh_env;
        }
        catch (...)
        {
            LOG_WARN("AX_FRONTAL_SCORE_THRESH 无效，使用默认 {}",
                     kDefaultFrontalScoreThresh);
        }
    }
    config_kv_["qdrant_collection"] = "face_embeddings";

    inited_ = true;
    return 0;
}

void HostServices::Shutdown()
{
    if (push_pool_)
    {
        push_pool_->Shutdown();
        push_pool_.reset();
    }
    face_server_.reset();
    client_.reset();
    engine_.reset();
    inited_ = false;
}

int HostServices::InferFaces(const ImageData& frame,
                             const std::vector<detection::Object>& faces,
                             std::vector<ImageData>& face_imgs,
                             std::vector<std::vector<float>>& feats)
{
    if (engine_ == nullptr || ivps_ == nullptr)
    {
        return -1;
    }
    return engine_->InferBatch(*ivps_, frame, faces, face_imgs, feats);
}

int HostServices::CloneFrame(ImageData& dest, const ImageData& src)
{
    return Clone(dest, const_cast<ImageData&>(src));
}

int HostServices::EncodeJpeg(std::vector<uint8_t>& dest, const ImageData& src)
{
    return JpegEncode(dest, src);
}

bool HostServices::CropFaceRoi(const ImageData& frame,
                               const detection::Object& face,
                               ImageData& face_img)
{
    std::lock_guard<std::mutex> lock(push_ivps_mutex_);
    face_img = ImageData{};
    if (push_ivps_ == nullptr || frame.data == nullptr)
    {
        return false;
    }
    cv::Mat aligned = face_align::HwRoiNormCrop(*push_ivps_, frame, face,
                                                face_img, 1.5f, 112);
    (void)aligned;
    return face_img.data != nullptr;
}

bool HostServices::EncodeVisitorFace(const ImageData& face_img,
                                     std::vector<uint8_t>& face_jpg)
{
    std::lock_guard<std::mutex> lock(push_ivps_mutex_);
    face_jpg.clear();
    if (push_ivps_ == nullptr || face_img.data == nullptr)
    {
        return false;
    }
    AX_S32 ret = push_ivps_->CropAndCSC(
        AX_FORMAT_YUV420_SEMIPLANAR, static_cast<AX_U16>(0),
        static_cast<AX_U16>(0), static_cast<AX_U16>(face_img.width),
        static_cast<AX_U16>(face_img.height));
    if (ret != 0)
    {
        LOG_ERROR("EncodeVisitorFace: CropAndCSC failed, ret={}", ret);
        return false;
    }

    ImageData yuv_frame;
    ret = push_ivps_->Process(yuv_frame, face_img);
    if (ret != 0)
    {
        LOG_ERROR("EncodeVisitorFace: IVPS Process failed, ret={}", ret);
        return false;
    }
    const int enc_ret = JpegEncode(face_jpg, yuv_frame);
    return enc_ret == 0 && !face_jpg.empty();
}

void HostServices::DrawRect(const ImageData& frame, int x1, int y1, int x2,
                            int y2, const YUVColor& color, int line_width)
{
    if (frame.data == nullptr || frame.data->FrameInfo() == nullptr)
    {
        return;
    }
    ::DrawRect(frame.data->FrameInfo(), x1, y1, x2, y2, color, line_width);
}

void HostServices::DrawText(const ImageData& frame, int x, int y,
                            const std::string& text, const YUVColor& color,
                            int font_size)
{
    if (frame.data == nullptr || frame.data->FrameInfo() == nullptr)
    {
        return;
    }
    ::DrawText(frame.data->FrameInfo(), x, y, text, color, font_size);
}

void HostServices::DrawCircle(const ImageData& frame, int cx, int cy, int radius,
                              const YUVColor& color)
{
    if (frame.data == nullptr || frame.data->FrameInfo() == nullptr)
    {
        return;
    }
    ::DrawCircle(frame.data->FrameInfo(), cx, cy, radius, color);
}

std::shared_ptr<HostServices::InflightGate>
HostServices::GetOrCreateGate(const std::string& name)
{
    std::lock_guard<std::mutex> lock(gates_mutex_);
    auto& g = gates_[name];
    if (!g)
    {
        g = std::make_shared<InflightGate>();
    }
    return g;
}

bool HostServices::SubmitAsync(const std::string& plugin_name,
                               std::function<void()> fn)
{
    if (!push_pool_ || !fn)
    {
        return false;
    }
    auto gate = GetOrCreateGate(plugin_name);
    gate->count.fetch_add(1, std::memory_order_acq_rel);
    auto submitted = push_pool_->Submit(
        [gate, job = std::move(fn)]() mutable {
            job();
            const int prev = gate->count.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1)
            {
                std::lock_guard<std::mutex> lock(gate->mu);
                gate->cv.notify_all();
            }
        });
    if (!submitted)
    {
        const int prev = gate->count.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1)
        {
            std::lock_guard<std::mutex> lock(gate->mu);
            gate->cv.notify_all();
        }
        return false;
    }
    return true;
}

bool HostServices::WaitQuiesce(const std::string& plugin_name, int timeout_ms)
{
    std::shared_ptr<InflightGate> gate;
    {
        std::lock_guard<std::mutex> lock(gates_mutex_);
        auto it = gates_.find(plugin_name);
        if (it == gates_.end() || !it->second)
        {
            return true;
        }
        gate = it->second;
    }
    std::unique_lock<std::mutex> lock(gate->mu);
    auto pred = [&] { return gate->count.load(std::memory_order_acquire) == 0; };
    if (timeout_ms < 0)
    {
        gate->cv.wait(lock, pred);
        return true;
    }
    return gate->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred);
}

qdrant::QdrantClient* HostServices::Vectors()
{
    return client_.get();
}

face_server::FaceServerClient* HostServices::Notify()
{
    return face_server_.get();
}

bool HostServices::VectorsReady() const
{
    return client_ != nullptr;
}

bool HostServices::NotifyReady() const
{
    return face_server_ != nullptr;
}

float HostServices::ConfigFloat(const char* key, float def) const
{
    auto it = config_kv_.find(key);
    if (it == config_kv_.end())
    {
        return def;
    }
    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        return def;
    }
}

std::string HostServices::ConfigString(const char* key,
                                       const std::string& def) const
{
    auto it = config_kv_.find(key);
    if (it == config_kv_.end())
    {
        return def;
    }
    return it->second;
}
