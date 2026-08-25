#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include "ax_global_type.h"
#include "image_data.h"
#include "ivps_helper.h"
#include "logger.h"
#include "task_scheduler.h"
#include "task_node.h"
#include "process_msg.h"
// SORT 实现已停用并移至 tmp/tracker_sort/（不参与编译，含已知越界缺陷），
// 当前跟踪走 BYTETracker.h。详见 tmp/tracker_sort/README.md。
#include "drawing.h"
#include "rule_engine.h"
#include "engine_factory.h"

#include "arcface.h"
#include "face_align.h"
#include "qdrant_client.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include "my_utils.h"
#include "thread_pool.h"
#include "face_server_client.h"
#include "BYTETracker.h"

using Json = nlohmann::json;
class BusProcess : public pipeline::TaskNode
{
public:
    BusProcess()
    {
        ivps_ = new IvpsHelper(1,
                               1920 * 1080 * 3,
                               32);
        push_ivps_ = new IvpsHelper(2,
                                    1920 * 1080 * 3,
                                    32);
        ArcfaceConfig config;
        engine_ = std::make_unique<Arcface>(config);
        if (engine_ != nullptr)
        {
            if (engine_->Init() != 0)
            {
                engine_ = nullptr;
            }
        }
    };

    ~BusProcess()
    {
        if (push_pool_)
        {
            push_pool_->Shutdown();
        }
        delete ivps_;
        delete push_ivps_;
    };

    int Init() override
    {
        // -1 会让后续 SendMessage 被 scheduler 静默拒绝，画面到不了编码器却无根因日志
        next_thread_id_ = pipeline::TaskNodeIdByName("EncProcess");
        if (next_thread_id_ < 0)
        {
            LOG_ERROR("BusProcess: 找不到下游节点 EncProcess");
            return -1;
        }

        qdrant::QdrantConfig config;
        config.host = "192.168.137.112";
        config.port = 6333;
        // config.api_key = "your-api-key"; // 若 Qdrant 开启了鉴权

        client_ = std::make_unique<qdrant::QdrantClient>(config);

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

        // 1. 创建 collection(若已存在则先删除重建)
        // auto create_res = client_->CreateCollection(collection_, vector_size_,
        //                                         "Cosine", true);
        // if (!create_res) {
        //     std::cerr << "创建 collection 失败: " << create_res.error << "\n";
        //     return 1;
        // }
        // std::cout << "创建 collection 成功\n";

        if (0 != ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, AX_FORMAT_RGB888, 112, 112))
        {
            LOG_ERROR("IVPS Init failed!");
            return -2;
        }
        if (0 != push_ivps_->Resize(AX_IVPS_ASPECT_RATIO_AUTO, AX_FORMAT_RGB888,
                                    112, 112))
        {
            LOG_ERROR("push IVPS Init failed!");
            return -2;
        }

        frontal_score_thresh_ = kDefaultFrontalScoreThresh;
        if (const char* thresh_env = std::getenv("AX_FRONTAL_SCORE_THRESH"))
        {
            try
            {
                frontal_score_thresh_ = std::stof(thresh_env);
            }
            catch (...)
            {
                LOG_WARN("AX_FRONTAL_SCORE_THRESH 无效，使用默认 {}",
                         kDefaultFrontalScoreThresh);
            }
        }
        // const std::string model_config = "configs/arcface.yaml";
        if (engine_ == nullptr)
        {
            LOG_ERROR("InfProcess: CreateEngine failed for");
            return -1;
        }

        // return engine_->Init();

        return 0;
    }

    int Start()
    {
        return 0;
    }

    int Process(int msg_id, std::shared_ptr<void> msg_data) override
    {
        int ret = 0;
        switch (msg_id)
        {
        case kMsgAppStart:
            Start();
            break;
        case kMsgInfprocData:
        {
            auto in_data = std::static_pointer_cast<InfData>(msg_data);

            // TIME_START(arcface);

            const std::string capture_msg_id = my_utils::GenerateUuid();
            const std::string cur_time = my_utils::GetCurrentTimeIso8601Utc();
            const std::int64_t cur_created_at = my_utils::GetUnixMilliseconds();
            // LOG_INFO("cur_time {}", cur_time);
            auto img_data = in_data->image;
            auto faces = in_data->objects;

            std::vector<std::vector<float> > feats;
            std::vector<ImageData> face_imgs;

            // 先把已完成的异步检索结果落账，再决定本帧哪些轨需要提特征。
            // track_pending_ 仅本线程改写，故其本身无需加锁。
            DrainIdentifyResults();

            // 全程跟踪全部检出框；正脸分仅门控 InferBatch，不前置过滤跟踪输入。
            // 加权分：ComputeFrontalScore(f, face_align::FrontalScoreRecommended())
            // 达标阈值 frontal_score_thresh_（默认 0.55，可用 AX_FRONTAL_SCORE_THRESH 覆盖）
            std::vector<detection::Object> tracked_faces = std::move(faces);
            // frontal_faces.reserve(faces.size());
            
            // for (const auto& f : faces)
            // {
            //     face_align::FrontalMetrics metrics;
            //     if (face_align::IsFrontalFace(
            //             f, face_align::FrontalRecommended(), &metrics))
            //     {
            //         frontal_faces.push_back(f);
            //     }
            //     else
            //     {
            //         // const auto& lm = f.landmark;
            //         // LOG_INFO(
            //         //     "skip non-frontal face: eye_dist={:.1f} roll={:.1f} "
            //         //     "yaw={:.2f} pitch={:.2f} sym={:.2f} valid={} | "
            //         //     "lm le=({:.1f},{:.1f}) re=({:.1f},{:.1f}) "
            //         //     "nose=({:.1f},{:.1f}) lmouth=({:.1f},{:.1f}) "
            //         //     "rmouth=({:.1f},{:.1f})",
            //         //     metrics.eye_dist, metrics.roll_deg, metrics.yaw_proxy,
            //         //     metrics.pitch_proxy, metrics.sym, metrics.valid,
            //         //     lm[0].x, lm[0].y, lm[1].x, lm[1].y, lm[2].x, lm[2].y,
            //         //     lm[3].x, lm[3].y, lm[4].x, lm[4].y);
            //     }
            // }

            // ------------------------------------------------------------------
            // ByteTrack 同轨去重 + 正脸分门控 Infer
            // 主线程：全部框 update -> IoU 回写 track_id -> 缓存未提特征轨的
            //         last_face/frame -> 正脸分达标才 InferBatch -> 组批交线程池。
            // 线程池：Qdrant 检索 / 判定 / 写库 / JPEG 编码 / 报警与访客推送，
            //         完成后把 {track_id, 成功与否} 回传，主线程下一帧落账。
            // 丢轨且从未提特征：PushAlert（整图+box）+ PushVisitor（手动裁脸），不写 Qdrant。
            // ------------------------------------------------------------------
            ++frame_seq_;
            const auto& frontal_score_cfg = face_align::FrontalScoreRecommended();
            // 仅返回 is_activated 的轨迹；未激活框本帧不处理，等后续帧激活。
            auto tracks = face_tracker_.update(tracked_faces);

            // 检测框(xywh) 与轨迹框(tlbr=x1y1x2y2) 的 IoU，用于把 track_id 挂回原 Object
            //（保留 landmark，供 ArcFace 对齐），阈值 0.3。
            auto rect_iou = [](const cv::Rect_<float>& a,
                               float x1, float y1, float x2, float y2) -> float {
                const float ax2 = a.x + a.width;
                const float ay2 = a.y + a.height;
                const float xx1 = std::max(a.x, x1);
                const float yy1 = std::max(a.y, y1);
                const float xx2 = std::min(ax2, x2);
                const float yy2 = std::min(ay2, y2);
                const float w = std::max(0.f, xx2 - xx1);
                const float h = std::max(0.f, yy2 - yy1);
                const float inter = w * h;
                const float uni = a.width * a.height +
                                 std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1) -
                                 inter;
                return uni > 0.f ? inter / uni : 0.f;
            };

            // 先清空，再按最大 IoU 一对一回写 track_id（保留 landmark 供对齐）
            for (auto& f : tracked_faces) {
                f.track_id = -1;
            }
            std::vector<char> face_used(tracked_faces.size(), 0);
            int matched = 0;
            for (const auto& t : tracks) {
                if (t.tlbr.size() < 4) {
                    continue;
                }
                int best_i = -1;
                float best_iou = 0.1f;  // 人脸框较小，阈值略放宽
                for (size_t i = 0; i < tracked_faces.size(); ++i) {
                    if (face_used[i]) {
                        continue;
                    }
                    const float iou = rect_iou(tracked_faces[i].rect, t.tlbr[0],
                                              t.tlbr[1], t.tlbr[2], t.tlbr[3]);
                    if (iou > best_iou) {
                        best_iou = iou;
                        best_i = static_cast<int>(i);
                    }
                }
                if (best_i >= 0) {
                    tracked_faces[best_i].track_id = t.track_id;
                    face_used[best_i] = 1;
                    ++matched;
                }
            }

            LOG_INFO("bytetrack: faces={} tracks={} matched={}",
                     tracked_faces.size(), tracks.size(), matched);

            // 跟踪器仍输出该轨时刷新 last_seen，避免检测偶发漏框导致过早丢轨推送
            for (const auto& t : tracks) {
                auto it = track_pending_.find(t.track_id);
                if (it != track_pending_.end()) {
                    it->second.last_seen_frame = frame_seq_;
                }
            }

            // 刷新未提特征轨的最近人脸与整帧缓存
            for (const auto& f : tracked_faces) {
                if (f.track_id < 0) {
                    continue;
                }
                auto& pending = track_pending_[f.track_id];
                pending.last_seen_frame = frame_seq_;
                if (!pending.feat_done) {
                    pending.last_face = f;
                    if (Clone(pending.last_frame, img_data) != 0) {
                        LOG_ERROR("Clone last_frame for track {} failed",
                                  f.track_id);
                        pending.last_frame = ImageData{};
                    }
                }
            }

            // 正脸分达标且尚未提特征的轨进入 InferBatch
            std::vector<detection::Object> to_process;
            to_process.reserve(tracked_faces.size());

            for (const auto& f : tracked_faces) {
                if (f.track_id < 0) {
                    continue;
                }
                auto& pending = track_pending_[f.track_id];
                if (pending.feat_done || pending.identify_inflight) {
                    continue;  // 已完成，或检索在途，避免同轨重复提特征
                }
                const float score = face_align::ComputeFrontalScore(
                    f, frontal_score_cfg, nullptr);
                if (score < frontal_score_thresh_) {
                    continue;
                }
                to_process.push_back(f);
            }


            // 正脸分达标轨进入 InferBatch；feat_done 在提取成功后再置位。
            engine_->InferBatch(*ivps_, img_data, to_process, face_imgs,
                                feats);

            LOG_INFO(
                "feats size {} (tracks {}/new {}/faces {} )",
                feats.size(), tracks.size(), to_process.size(),
                tracked_faces.size());

            // 组批：推理成功的轨交由线程池做检索/判定/写库/推送。
            // 主线程只置 identify_inflight 并保留 last_frame 作兜底，不接触网络。
            std::vector<IdentifyItem> identify_batch;
            identify_batch.reserve(feats.size());

            for (size_t i = 0; i < feats.size(); ++i)
            {
                if (feats[i].empty())
                {
                    if (i < to_process.size())
                    {
                        LOG_WARN("Infer 空特征 track_id={} score_gate={}",
                                 to_process[i].track_id,
                                 frontal_score_thresh_);
                    }
                    continue;  // 未置 inflight，后续帧仍可重试
                }
                if (i >= to_process.size() || i >= face_imgs.size())
                {
                    continue;
                }

                const int tid = to_process[i].track_id;
                auto pit = track_pending_.find(tid);
                if (pit == track_pending_.end())
                {
                    continue;
                }
                pit->second.identify_inflight = true;

                IdentifyItem item;
                item.track_id = tid;
                item.face = to_process[i];
                item.feat = std::move(feats[i]);
                item.face_img = face_imgs[i];
                identify_batch.push_back(std::move(item));
            }

            if (!identify_batch.empty())
            {
                // batch 会被 move 进 lambda，先留一份 id 以便失败时回滚。
                // 任何不进线程池的分支都必须回滚，否则该轨永久卡在 inflight：
                // 既不会 feat_done，prune 循环也永不 erase，track_pending_ 会无界增长。
                std::vector<int> inflight_ids;
                inflight_ids.reserve(identify_batch.size());
                for (const auto& item : identify_batch)
                {
                    inflight_ids.push_back(item.track_id);
                }

                ImageData frame_copy;
                if (!client_ || !face_server_ || !push_pool_)
                {
                    LOG_ERROR("检索依赖未就绪，跳过 count={}",
                              inflight_ids.size());
                    RollbackInflight(inflight_ids);
                }
                // 整帧与后续 Draw/Enc 共享缓冲，必须深拷贝。
                else if (Clone(frame_copy, img_data) != 0 ||
                         frame_copy.data == nullptr)
                {
                    LOG_ERROR("Clone frame for identify failed");
                    RollbackInflight(inflight_ids);
                }
                else
                {
                    // 捕获 this 安全：析构体内先 Shutdown 线程池并 join 所有 worker。
                    auto submitted = push_pool_->Submit(
                        [this, capture_msg_id, cur_time, cur_created_at,
                         batch = std::move(identify_batch),
                         frame = std::move(frame_copy)]() mutable {
                            RunIdentifyAndPush(std::move(batch),
                                               std::move(frame), capture_msg_id,
                                               cur_time, cur_created_at);
                        });
                    if (!submitted)
                    {
                        LOG_ERROR("检索推送丢弃: 线程池队列已满 count={}",
                                  inflight_ids.size());
                        RollbackInflight(inflight_ids);
                    }
                }
            }

            // 丢轨且从未提特征：先裁脸再报警+访客，不写 Qdrant；Submit 失败保留条目重试
            for (auto it = track_pending_.begin(); it != track_pending_.end();) {
                if (frame_seq_ - it->second.last_seen_frame <= kTrackPruneFrames) {
                    ++it;
                    continue;
                }
                const int lost_track_id = it->first;
                if (it->second.feat_done) {
                    it = track_pending_.erase(it);
                    continue;
                }
                if (it->second.identify_inflight) {
                    // 检索在途：等结果落账后再决定是否需要兜底推送，避免与
                    // worker 里的陌生人推送重复，也避免检索失败时漏推。
                    // 超时兜底闸：结果永远没回来（worker 异常等）时释放条目，
                    // 否则它会一直占着一份整帧克隆。
                    if (frame_seq_ - it->second.last_seen_frame >
                        kTrackInflightMaxFrames) {
                        LOG_ERROR("检索结果超时未回传，丢弃条目 track_id={}",
                                  lost_track_id);
                        it = track_pending_.erase(it);
                    } else {
                        ++it;
                    }
                    continue;
                }

                TrackPending pending = it->second;
                if (pending.last_frame.data == nullptr || !face_server_ ||
                    !push_pool_) {
                    it = track_pending_.erase(it);
                    continue;
                }

                ImageData frame_copy;
                if (Clone(frame_copy, pending.last_frame) != 0 ||
                    frame_copy.data == nullptr) {
                    LOG_ERROR("Clone lost-track frame failed track_id={}",
                              lost_track_id);
                    it = track_pending_.erase(it);
                    continue;
                }

                const detection::Object face_copy = pending.last_face;
                const std::string lost_msg_id = my_utils::GenerateUuid();
                const std::string lost_time = my_utils::GetCurrentTimeIso8601Utc();
                const std::string alert_description =
                    MakeStrangerAlertDescription(lost_track_id, face_copy);
                face_server::FaceServerClient* client = face_server_.get();

                const auto submitted = push_pool_->Submit(
                    [this, client, lost_msg_id, lost_time, lost_track_id,
                     alert_description, face_copy,
                     frame = std::move(frame_copy)]() mutable {
                        ImageData face_roi;
                        if (!CropFaceRoiLocked(frame, face_copy, face_roi)) {
                            LOG_ERROR("lost-track crop face failed track_id={}",
                                      lost_track_id);
                            return;
                        }

                        std::vector<uint8_t> face_jpg;
                        if (!EncodeVisitorFaceLocked(face_roi, face_jpg)) {
                            LOG_ERROR(
                                "lost-track face JpegEncode failed track_id={}",
                                lost_track_id);
                            return;
                        }

                        std::vector<uint8_t> frame_jpg;
                        if (JpegEncode(frame_jpg, frame) != 0 ||
                            frame_jpg.empty()) {
                            LOG_ERROR("lost-track JpegEncode frame failed");
                            return;
                        }

                        face_server::ImageBlob ori_img;
                        ori_img.content_type = "image/jpeg";
                        ori_img.filename = lost_msg_id + ".jpeg";
                        ori_img.data = std::move(frame_jpg);

                        face_server::AlertPushRequest alert_req;
                        alert_req.bank_id = "test_bank_id";
                        alert_req.msg_id = lost_msg_id;
                        alert_req.event_id = "stat_id";
                        alert_req.org_id = "test_org_id";
                        alert_req.event_time = lost_time;
                        alert_req.event_name = "陌生人闯入";
                        alert_req.channel_name = "test_channel_name";
                        alert_req.description = alert_description;
                        alert_req.files = {ori_img};

                        auto alert_res = client->PushAlert(alert_req);
                        if (!alert_res) {
                            LOG_ERROR("lost-track PushAlert 失败: {}",
                                      alert_res.error);
                        }

                        face_server::VisitorPushRequest visitor_req;
                        visitor_req.msg_id = lost_msg_id;
                        visitor_req.event_time = lost_time;
                        visitor_req.camera_name = "test_camera";
                        visitor_req.original = ori_img;

                        face_server::VisitorPerson person;
                        person.ehr_no = "NA";
                        person.name = "NA";
                        person.recognized = false;
                        person.msg_id = lost_msg_id;
                        person.event_id = "stranger";
                        visitor_req.persons.push_back(person);

                        face_server::ImageBlob face_img;
                        face_img.content_type = "image/jpeg";
                        face_img.filename = lost_msg_id + "_0.jpeg";
                        face_img.data = std::move(face_jpg);
                        visitor_req.faces.push_back(std::move(face_img));

                        auto vis_res = client->PushVisitor(visitor_req);
                        if (!vis_res) {
                            LOG_ERROR("lost-track PushVisitor 失败: {}",
                                      vis_res.error);
                        }
                    });
                if (submitted) {
                    it = track_pending_.erase(it);
                } else {
                    LOG_ERROR(
                        "丢轨陌生人推送丢弃: 线程池队列已满 track_id={}",
                        lost_track_id);
                    ++it;
                }
            }

            // TIME_END(arcface);
            // TIME_USEC_SHOW(arcface);

            if (EnsureMapped(in_data->image) != 0)
            {
                LOG_ERROR("BusProcess EnsureMapped failed, skip draw");
            }
            else if (in_data->image.data == nullptr || in_data->image.data->FrameInfo() == nullptr || in_data->image.data->FrameInfo()->stVFrame.u64VirAddr[0] == 0)
            {
                LOG_ERROR("BusProcess mapped frame invalid, skip draw");
                Unmap(in_data->image);
            }
            else
            {
                // 画当前帧人脸：有 track_id 用红框；未匹配到轨用黄框标 -1（便于确认是否在跟踪）。
                // auto* frame = in_data->image.data->FrameInfo();
                // for (size_t i = 0; i < frontal_faces.size(); i++)
                // {
                //     const auto& item = frontal_faces[i];
                //     const bool tracked = item.track_id >= 0;
                //     const auto color = tracked ? YUVColors::kRed : YUVColors::kYellow;
                //     std::string txt = std::to_string(item.track_id) + " " +
                //                       std::to_string(item.prob);
                //     DrawText(frame, item.rect.x, item.rect.y + 5, txt, color, 32);
                //     DrawRect(frame,
                //              static_cast<int>(item.rect.x),
                //              static_cast<int>(item.rect.y),
                //              static_cast<int>(item.rect.x + item.rect.width),
                //              static_cast<int>(item.rect.y + item.rect.height),
                //              color, 2);

                //     // Scrfd 5pts: le, re, nose, lmouth, rmouth
                //     static const YUVColor kLmColors[5] = {
                //         YUVColors::kGreen, YUVColors::kBlue, YUVColors::kYellow,
                //         YUVColors::kCyan, YUVColors::kMagenta,
                //     };
                //     constexpr int kLmRadius = 3;
                //     for (int k = 0; k < 5; ++k) {
                //         DrawCircle(frame,
                //                    static_cast<int>(item.landmark[k].x),
                //                    static_cast<int>(item.landmark[k].y),
                //                    kLmRadius, kLmColors[k]);
                //     }
                // }
                Unmap(in_data->image);
            }

            auto out_data = std::make_shared<BusData>();
            out_data->image = in_data->image;
            ret = pipeline::SendMessage(next_thread_id_, kMsgBusprocData, out_data);

            break;
        }
        case kMsgAppExit:
            // RuleEngine::Instance().Unload();
            break;
        default:
            break;
        }
        return ret;
    }

private:
    struct TrackPending {
        detection::Object last_face;
        ImageData last_frame;
        // 检索已完成（无论命中与否），该轨不再提特征、不再兜底推送
        bool feat_done = false;
        // 特征已提取、检索在线程池中进行；期间不重复提特征也不兜底推送
        bool identify_inflight = false;
        int last_seen_frame = 0;
    };

    // 主线程 -> worker：一次检索所需的全部输入
    struct IdentifyItem {
        int track_id = -1;
        detection::Object face;   // rect 用于报警 box，landmark 已用于对齐
        std::vector<float> feat;
        ImageData face_img;       // ArcFace 裁好的 ROI，供访客推送编码
    };

    // worker -> 主线程：检索是否完成。ok=false 表示网络失败，允许后续帧重试
    struct IdentifyResult {
        int track_id = -1;
        bool ok = false;
    };

    void PostIdentifyResult(int track_id, bool ok)
    {
        std::lock_guard<std::mutex> lock(identify_results_mutex_);
        identify_results_.push_back({track_id, ok});
    }

    // 仅主线程调用：把 worker 回传的结果落到 track_pending_
    void DrainIdentifyResults()
    {
        std::vector<IdentifyResult> results;
        {
            std::lock_guard<std::mutex> lock(identify_results_mutex_);
            results.swap(identify_results_);
        }
        for (const auto& r : results)
        {
            auto it = track_pending_.find(r.track_id);
            if (it == track_pending_.end())
            {
                continue;  // 该轨已被 prune
            }
            it->second.identify_inflight = false;
            if (r.ok)
            {
                it->second.feat_done = true;
                it->second.last_frame = ImageData{};  // 不再需要兜底整帧
            }
            else
            {
                LOG_WARN("检索失败，track_id={} 允许后续帧重试", r.track_id);
            }
        }
    }

    // 仅主线程调用：Submit 失败时撤销 inflight 标记
    void RollbackInflight(const std::vector<int>& track_ids)
    {
        for (int tid : track_ids)
        {
            auto it = track_pending_.find(tid);
            if (it != track_pending_.end())
            {
                it->second.identify_inflight = false;
            }
        }
    }

    // ---- 以下在推送线程池中执行 ----

    // push_ivps_ 由多个 worker 共用，IvpsHelper 内部无锁且 CropAndCSC 会重配
    // 硬件 pipeline，故所有硬件调用必须串行化。耗时仅 ms 级，不阻塞网络部分。
    bool EncodeVisitorFaceLocked(const ImageData& face_img,
                                 std::vector<uint8_t>& face_jpg)
    {
        std::lock_guard<std::mutex> lock(push_ivps_mutex_);
        return EncodeVisitorFaceJpeg(push_ivps_, face_img, face_jpg);
    }

    bool CropFaceRoiLocked(const ImageData& frame,
                           const detection::Object& face, ImageData& face_img)
    {
        std::lock_guard<std::mutex> lock(push_ivps_mutex_);
        return CropFaceRoiFromFrame(push_ivps_, frame, face, face_img);
    }

    static std::string PayloadString(const Json& payload, const char* key,
                                     const std::string& def)
    {
        if (!payload.contains(key))
        {
            return def;
        }
        const auto& v = payload[key];
        if (v.is_string())
        {
            return v.get<std::string>();
        }
        if (v.is_number_integer())
        {
            return std::to_string(v.get<int64_t>());
        }
        return def;
    }

    // 检索 -> 判定 -> 写库 -> 编码 -> 报警/访客推送，全程不占用主管线线程。
    void RunIdentifyAndPush(std::vector<IdentifyItem> batch, ImageData frame,
                            const std::string& capture_msg_id,
                            const std::string& cur_time,
                            std::int64_t cur_created_at)
    {
        bool is_send_alert = false;
        std::vector<qdrant::Point> points;
        std::vector<std::pair<int, detection::Object>> alert_stranger_faces;

        face_server::VisitorPushRequest visitor_req;
        visitor_req.msg_id = capture_msg_id;
        visitor_req.event_time = cur_time;
        visitor_req.camera_name = "test_camera";
        std::vector<ImageData> visitor_faces;

        for (auto& item : batch)
        {
            auto search_res = client_->Search(collection_, item.feat, 1, {},
                                              true, false, 0.65f);
            if (!search_res)
            {
                // 不置 feat_done：主线程回滚后该轨可在后续帧重试
                LOG_ERROR("检索失败 track_id={}: {}", item.track_id,
                          search_res.error);
                PostIdentifyResult(item.track_id, false);
                continue;
            }

            PostIdentifyResult(item.track_id, true);

            std::string ehr_no = "NA";
            std::string name = "NA";
            if (search_res.points.empty())
            {
                LOG_INFO("未命中：陌生人 track_id={}", item.track_id);
            }
            else
            {
                const auto& point = search_res.points.front();
                LOG_INFO("检索命中 id={} score={}", point.id, point.score);
                ehr_no = PayloadString(point.payload, "ehrNo", "NA");
                name = PayloadString(point.payload, "name", "NA");
            }

            // ehrNo=="NA" 覆盖两种情况：库里没有，或库里存的就是陌生人记录
            const bool is_stranger = (ehr_no == "NA");
            if (is_stranger)
            {
                is_send_alert = true;
                const auto uuid = my_utils::GenerateUuid();
                points.push_back(qdrant::Point::WithStringId(
                    uuid, item.feat,
                    Json{{"createdAt", cur_created_at},
                         {"name", "NA"},
                         {"ehrNo", "NA"}}));
                alert_stranger_faces.push_back({item.track_id, item.face});
            }

            face_server::VisitorPerson person;
            person.ehr_no = is_stranger ? "NA" : ehr_no;
            person.name = is_stranger ? "NA" : name;
            person.recognized = !is_stranger;
            person.msg_id = capture_msg_id;
            person.event_id = is_stranger ? "stranger" : "visitor";
            visitor_req.persons.push_back(person);
            visitor_faces.push_back(item.face_img);
        }

        if (!points.empty())
        {
            auto upsert_res = client_->UpsertPoints(collection_, points);
            if (!upsert_res)
            {
                LOG_ERROR("写入点失败: {}", upsert_res.error);
            }
        }

        const bool need_visitor =
            !visitor_req.persons.empty() &&
            visitor_faces.size() == visitor_req.persons.size();
        if (!visitor_req.persons.empty() && !need_visitor)
        {
            LOG_ERROR("visitor person and face not eq {}=={}",
                      visitor_faces.size(), visitor_req.persons.size());
        }
        if (!is_send_alert && !need_visitor)
        {
            return;
        }

        std::vector<uint8_t> frame_jpg;
        if (JpegEncode(frame_jpg, frame) != 0 || frame_jpg.empty())
        {
            LOG_ERROR("identify JpegEncode frame failed");
            return;
        }

        face_server::ImageBlob ori_img;
        ori_img.content_type = "image/jpeg";
        ori_img.filename = capture_msg_id + ".jpeg";
        ori_img.data = std::move(frame_jpg);

        if (is_send_alert)
        {
            face_server::AlertPushRequest alert_req;
            alert_req.bank_id = "test_bank_id";
            alert_req.msg_id = capture_msg_id;
            alert_req.event_id = "stat_id";
            alert_req.org_id = "test_org_id";
            alert_req.event_time = cur_time;
            alert_req.event_name = "陌生人闯入";
            alert_req.channel_name = "test_channel_name";
            alert_req.description =
                MakeStrangerAlertDescription(alert_stranger_faces);
            alert_req.files = {ori_img};

            auto alert_res = face_server_->PushAlert(alert_req);
            if (!alert_res)
            {
                LOG_ERROR("PushAlert 失败: {}", alert_res.error);
            }
        }

        if (need_visitor)
        {
            visitor_req.original = ori_img;
            visitor_req.faces.clear();
            visitor_req.faces.reserve(visitor_faces.size());

            for (size_t j = 0; j < visitor_faces.size(); ++j)
            {
                std::vector<uint8_t> face_jpg;
                if (!EncodeVisitorFaceLocked(visitor_faces[j], face_jpg))
                {
                    LOG_ERROR("face JpegEncode Failed j={}", j);
                    visitor_req.faces.clear();
                    break;
                }

                face_server::ImageBlob img;
                img.content_type = "image/jpeg";
                img.filename =
                    capture_msg_id + "_" + std::to_string(j) + ".jpeg";
                img.data = std::move(face_jpg);
                visitor_req.faces.push_back(std::move(img));
            }

            if (visitor_req.faces.size() == visitor_req.persons.size())
            {
                auto vis_res = face_server_->PushVisitor(visitor_req);
                if (!vis_res)
                {
                    LOG_ERROR("PushVisitor 失败: {}", vis_res.error);
                }
            }
        }
    }

    static Json MakeStrangerBoxJson(const detection::Object& face)
    {
        return Json{{"x", face.rect.x},
                    {"y", face.rect.y},
                    {"width", face.rect.width},
                    {"height", face.rect.height}};
    }

    static std::string MakeStrangerAlertDescription(int track_id,
                                                    const detection::Object& face)
    {
        Json desc;
        desc["event"] = "陌生人闯入";
        if (track_id >= 0)
        {
            desc["track_id"] = track_id;
        }
        desc["box"] = MakeStrangerBoxJson(face);
        return desc.dump();
    }

    static std::string MakeStrangerAlertDescription(
        const std::vector<std::pair<int, detection::Object>>& strangers)
    {
        if (strangers.empty())
        {
            return {};
        }
        if (strangers.size() == 1)
        {
            return MakeStrangerAlertDescription(strangers.front().first,
                                                strangers.front().second);
        }
        Json desc;
        desc["event"] = "陌生人闯入";
        Json faces = Json::array();
        for (const auto& item : strangers)
        {
            Json entry;
            if (item.first >= 0)
            {
                entry["track_id"] = item.first;
            }
            entry["box"] = MakeStrangerBoxJson(item.second);
            faces.push_back(std::move(entry));
        }
        desc["faces"] = std::move(faces);
        return desc.dump();
    }

    static bool EncodeVisitorFaceJpeg(IvpsHelper* ivps,
                                      const ImageData& face_img,
                                      std::vector<uint8_t>& face_jpg)
    {
        face_jpg.clear();
        if (ivps == nullptr || face_img.data == nullptr) {
            return false;
        }
        AX_S32 ret = ivps->CropAndCSC(
            AX_FORMAT_YUV420_SEMIPLANAR, static_cast<AX_U16>(0),
            static_cast<AX_U16>(0), static_cast<AX_U16>(face_img.width),
            static_cast<AX_U16>(face_img.height));
        if (ret != 0) {
            LOG_ERROR("EncodeVisitorFaceJpeg: CropAndCSC failed, ret={}", ret);
            return false;
        }

        ImageData yuv_frame;
        ret = ivps->Process(yuv_frame, face_img);
        if (ret != 0) {
            LOG_ERROR("EncodeVisitorFaceJpeg: IVPS Process failed, ret={}",
                      ret);
            return false;
        }

        const int enc_ret = JpegEncode(face_jpg, yuv_frame);
        return enc_ret == 0 && !face_jpg.empty();
    }

    static bool CropFaceRoiFromFrame(IvpsHelper* ivps, const ImageData& frame,
                                     const detection::Object& face,
                                     ImageData& face_img)
    {
        face_img = ImageData{};
        if (ivps == nullptr || frame.data == nullptr) {
            return false;
        }
        cv::Mat aligned = face_align::HwRoiNormCrop(
            *ivps, frame, face, face_img, 1.5f, 112);
        (void)aligned;
        return face_img.data != nullptr;
    }

    // ---- ByteTrack 同轨去重状态 ----
    // face_tracker_：按帧关联全部人脸框，输出稳定 track_id（构造参数：fps, track_buffer）
    BYTETracker face_tracker_{25, 30};
    // track_id -> 未提特征轨的最近人脸/整帧；feat_done 后不再缓存帧
    // 只在管线线程读写，worker 通过 identify_results_ 回传，不直接访问
    std::unordered_map<int, TrackPending> track_pending_;
    int frame_seq_ = 0;

    // worker -> 主线程的检索结果回传队列
    std::mutex identify_results_mutex_;
    std::vector<IdentifyResult> identify_results_;
    // 串行化 push_ivps_ 的硬件调用（2 个 worker 共用一个 IvpsHelper）
    std::mutex push_ivps_mutex_;
    // 未见超过该帧数则 prune（约 2 * track_buffer）
    static constexpr int kTrackPruneFrames = 60;
    // 丢轨后检索仍未回传的最长等待帧数（25fps 约 60s，远超 Qdrant 重试最坏耗时）
    static constexpr int kTrackInflightMaxFrames = 1500;
    static constexpr float kDefaultFrontalScoreThresh = 0.55f;
    float frontal_score_thresh_ = kDefaultFrontalScoreThresh;

    IvpsHelper* ivps_ = nullptr;
    IvpsHelper* push_ivps_ = nullptr;
    std::unique_ptr<Arcface> engine_;
    // uint64_t frame_id_ = 0;
    int next_thread_id_ = -1;
    const std::string collection_ = "face_embeddings";
    // const int vector_size_ = 512;
    std::unique_ptr<qdrant::QdrantClient> client_;
    // Destroy push_pool_ before face_server_ (declaration order reversed).
    std::unique_ptr<face_server::FaceServerClient> face_server_;
    std::unique_ptr<my_utils::ThreadPool> push_pool_;
};
