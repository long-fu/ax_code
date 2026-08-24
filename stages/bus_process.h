#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <fstream>
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
// #include "sort_track.h"
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
    };

    int Init() override
    {
        next_thread_id_ = pipeline::TaskNodeIdByName("EncProcess");

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

            face_server::VisitorPushRequest visitor_req;
            std::vector<ImageData> visitor_faces;
            
            visitor_req.msg_id = capture_msg_id;
            visitor_req.event_time = cur_time;
            visitor_req.camera_name = "test_camera";

            bool is_send_alert = false;
            std::vector<qdrant::Point> points;

            auto make_stranger_payload = [cur_created_at](const std::string& uuid) {
                return Json{{"createdAt", cur_created_at},
                            {"name", "NA"},
                            {"ehrNo", "NA"}};
            };

            auto payload_string = [](const Json& payload, const char* key,
                                     const std::string& def) -> std::string {
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
            };

            auto payload_int64 = [](const Json& payload, const char* key,
                                     std::int64_t def) -> std::int64_t {
                if (!payload.contains(key))
                {
                    return def;
                }
                const auto& v = payload[key];
                if (v.is_number_integer())
                {
                    return v.get<std::int64_t>();
                }
                if (v.is_number_unsigned())
                {
                    return static_cast<std::int64_t>(v.get<std::uint64_t>());
                }
                if (v.is_string())
                {
                    try
                    {
                        return std::stoll(v.get<std::string>());
                    }
                    catch (...)
                    {
                        return def;
                    }
                }
                return def;
            };

            // 全程跟踪全部检出框；正脸分仅门控 InferBatch，不前置过滤跟踪输入。
            // 加权分：ComputeFrontalScore(f, face_align::FrontalScoreRecommended())
            // 达标阈值 kFrontalScoreThresh（默认 0.55）；硬门限见 face_align.h。
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
            // 流程：全部框 update -> IoU 回写 track_id -> 缓存未提特征轨的 last_face/frame
            //      -> 正脸分达标且未 feat_done 才 InferBatch / 检索 / 上送
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
                if (pending.feat_done) {
                    continue;
                }
                const float score = face_align::ComputeFrontalScore(
                    f, frontal_score_cfg, nullptr);
                if (score < kFrontalScoreThresh) {
                    continue;
                }
                to_process.push_back(f);
                pending.feat_done = true;
                pending.last_frame = ImageData{};
            }


            // 仅对新出现的轨提特征；to_process 为空时 InferBatch 应为空操作。
            engine_->InferBatch(*ivps_, img_data, to_process, face_imgs,
                                feats);

            LOG_INFO(
                "feats size {} (tracks {}/new {}/faces {} )",
                feats.size(), tracks.size(), to_process.size(),
                tracked_faces.size());

            for (size_t i = 0; i < feats.size(); ++i)
            {
                auto& feat = feats[i];
                if (feat.empty())
                {
                    continue;
                }

                // 短时重复触发已由 ByteTrack track_id 在 InferBatch 前过滤；此处仅做身份检索与上送。

                auto search_res = client_->Search(collection_, feat, 1, {},
                                                  true, false, 0.65f);

                if (!search_res)
                {
                    LOG_ERROR("检索失败: {}", search_res.error);
                    continue;
                }
                
                // 
                if (search_res.points.empty())
                {
                    // 未命中：陌生人
                    LOG_INFO("未命中：陌生人");

                    is_send_alert = true;
                    const auto uuid = my_utils::GenerateUuid();
                    points.push_back(qdrant::Point::WithStringId(
                        uuid, feat, make_stranger_payload(uuid)));

                    face_server::VisitorPerson person;
                    person.ehr_no = "NA";
                    person.name = "NA";
                    person.recognized = false;
                    person.msg_id = capture_msg_id;
                    person.event_id = "stranger";
                    visitor_req.persons.push_back(person);
                    visitor_faces.push_back(face_imgs[i]);
                    continue;
                }

                const auto& point = search_res.points.front();
                LOG_INFO("检索命中 id={} score={}", point.id, point.score);
                const std::string ehr_no = payload_string(point.payload, "ehrNo", "NA");
                const std::string name = payload_string(point.payload, "name", "NA");
                // const std::int64_t created_at =
                    // payload_int64(point.payload, "createdAt", 0);


                if (ehr_no == "NA")
                {
                    // 库中是陌生人记录：允许再次上送（新轨场景下由 track 去重控制频率）
                    LOG_INFO("命中：陌生人");

                    // 命中陌生人 使用时间 过滤器能二次命中到吗, 


                    is_send_alert = true;
                    const auto uuid = my_utils::GenerateUuid();
                    points.push_back(qdrant::Point::WithStringId(
                        uuid, feat, make_stranger_payload(uuid)));

                    face_server::VisitorPerson person;
                    person.ehr_no = "NA";
                    person.name = "NA";
                    person.recognized = false;
                    person.msg_id = capture_msg_id;
                    person.event_id = "stranger";
                    visitor_req.persons.push_back(person);
                    visitor_faces.push_back(face_imgs[i]);


                }
                else
                {
                    // 访客命中：短时同人频率已由 track_id 控制，此处直接组包上送
                    LOG_INFO("命中：访客人员");
                    face_server::VisitorPerson person;
                    person.ehr_no = ehr_no;
                    person.name = name;
                    person.recognized = true;
                    person.msg_id = capture_msg_id;
                    person.event_id = "visitor";
                    visitor_req.persons.push_back(person);
                    visitor_faces.push_back(face_imgs[i]);

                }
            }

            if (!points.empty())
            {
                auto upsert_res = client_->UpsertPoints(collection_, points);
                if (!upsert_res)
                {
                    LOG_ERROR("写入点失败: {}", upsert_res.error);
                }
            }

            // LOG_INFO("visitor person info {} , {}", visitor_faces.size(),
            //          visitor_req.persons.size());

            const bool need_visitor = !visitor_req.persons.empty() && visitor_faces.size() == visitor_req.persons.size();

            if (!visitor_req.persons.empty() && !need_visitor)
            {
                LOG_ERROR("visitor person and face not eq {}=={}",
                          visitor_faces.size(), visitor_req.persons.size());
            }

            const bool need_push = (is_send_alert || need_visitor) && face_server_ && push_pool_;

            if (need_push)
            {
                LOG_INFO("需要推送xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

                // 整帧与后续 Draw/Enc 共享缓冲，必须深拷贝。
                ImageData frame_copy;
                if (Clone(frame_copy, img_data) != 0 || frame_copy.data == nullptr)
                {
                    LOG_ERROR("Clone frame for push failed");
                }
                else
                {
                    // 人脸 ROI 由 shared_ptr 持有 FrameData，传值即可拖住释放。
                    std::vector<ImageData> faces;
                    if (need_visitor)
                    {
                        faces = std::move(visitor_faces);
                    }

                    face_server::FaceServerClient* client = face_server_.get();
                    const bool send_alert = is_send_alert;
                    face_server::VisitorPushRequest vis_req;
                    if (need_visitor)
                    {
                        vis_req = std::move(visitor_req);
                    }

                    auto ivps = ivps_;

                    auto submitted = push_pool_->Submit(
                        [ivps, client, send_alert, capture_msg_id, cur_time,
                         frame = std::move(frame_copy),
                         faces = std::move(faces),
                         visitor_req = std::move(vis_req)]() mutable {
                            std::vector<uint8_t> frame_jpg;

                            // TIME_START(JpegEncode);

                            const int jpeg_ret = JpegEncode(frame_jpg, frame);

                            // TIME_END(JpegEncode);
                            // TIME_USEC_SHOW(JpegEncode); 4ms

                            if (jpeg_ret != 0 || frame_jpg.empty())
                            {
                                LOG_ERROR("JpegEncode frame Failed ret={}",
                                          jpeg_ret);
                                return;
                            }

                            face_server::ImageBlob ori_img;
                            ori_img.content_type = "image/jpeg";
                            ori_img.filename = capture_msg_id + ".jpeg";
                            ori_img.data = std::move(frame_jpg);

                            if (send_alert)
                            {
                                face_server::AlertPushRequest alert_req;
                                alert_req.bank_id = "test_bank_id";
                                alert_req.msg_id = capture_msg_id;
                                alert_req.event_id = "stat_id";
                                alert_req.org_id = "test_org_id";
                                alert_req.event_time = cur_time;
                                alert_req.event_name = "陌生人闯入";
                                alert_req.channel_name = "test_channel_name";
                                alert_req.description = "陌生人闯入";
                                alert_req.files = {ori_img};

                                auto alert_res = client->PushAlert(alert_req);
                                if (!alert_res)
                                {
                                    LOG_ERROR("PushAlert 失败: {}",
                                              alert_res.error);
                                }
                            }

                            if (!visitor_req.persons.empty())
                            {
                                visitor_req.original = ori_img;
                                visitor_req.faces.clear();
                                visitor_req.faces.reserve(faces.size());

                                for (size_t j = 0; j < faces.size(); ++j)
                                {
                                    std::vector<uint8_t> face_jpg;
                                    if (!EncodeVisitorFaceJpeg(ivps, faces[j],
                                                               face_jpg))
                                    {
                                        LOG_ERROR(
                                            "face JpegEncode Failed j={}", j);
                                        visitor_req.faces.clear();
                                        break;
                                    }

                                    face_server::ImageBlob img;
                                    img.content_type = "image/jpeg";
                                    img.filename = capture_msg_id + "_" + std::to_string(j) + ".jpeg";
                                    img.data = std::move(face_jpg);
                                    visitor_req.faces.push_back(
                                        std::move(img));
                                }

                                if (visitor_req.faces.size() == visitor_req.persons.size())
                                {
                                    auto vis_res = client->PushVisitor(visitor_req);
                                    if (!vis_res)
                                    {
                                        LOG_ERROR("PushVisitor 失败: {}",
                                                  vis_res.error);
                                    }
                                }
                            }
                        });
                    if (!submitted)
                    {
                        LOG_ERROR(
                            "推送丢弃: 线程池队列已满 (alert={} visitor={})",
                            is_send_alert, need_visitor);
                    }
                }
            }

            // 丢轨且从未提特征：报警整图+box，访客手动裁脸一次，不写 Qdrant
            for (auto it = track_pending_.begin(); it != track_pending_.end();) {
                if (frame_seq_ - it->second.last_seen_frame <= kTrackPruneFrames) {
                    ++it;
                    continue;
                }
                TrackPending pending = std::move(it->second);
                const int lost_track_id = it->first;
                it = track_pending_.erase(it);

                if (pending.feat_done || pending.last_frame.data == nullptr ||
                    !face_server_ || !push_pool_) {
                    continue;
                }

                ImageData frame_copy;
                if (Clone(frame_copy, pending.last_frame) != 0 ||
                    frame_copy.data == nullptr) {
                    LOG_ERROR("Clone lost-track frame failed track_id={}",
                              lost_track_id);
                    continue;
                }

                const detection::Object face_copy = pending.last_face;
                const std::string lost_msg_id = my_utils::GenerateUuid();
                const std::string lost_time = my_utils::GetCurrentTimeIso8601Utc();
                face_server::FaceServerClient* client = face_server_.get();
                auto ivps = ivps_;

                const auto submitted = push_pool_->Submit(
                    [ivps, client, lost_msg_id, lost_time, lost_track_id,
                     face_copy, frame = std::move(frame_copy)]() mutable {
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

                        Json desc;
                        desc["event"] = "陌生人闯入";
                        desc["track_id"] = lost_track_id;
                        desc["box"] = {
                            {"x", face_copy.rect.x},
                            {"y", face_copy.rect.y},
                            {"width", face_copy.rect.width},
                            {"height", face_copy.rect.height},
                        };

                        face_server::AlertPushRequest alert_req;
                        alert_req.bank_id = "test_bank_id";
                        alert_req.msg_id = lost_msg_id;
                        alert_req.event_id = "stat_id";
                        alert_req.org_id = "test_org_id";
                        alert_req.event_time = lost_time;
                        alert_req.event_name = "陌生人闯入";
                        alert_req.channel_name = "test_channel_name";
                        alert_req.description = desc.dump();
                        alert_req.files = {ori_img};

                        auto alert_res = client->PushAlert(alert_req);
                        if (!alert_res) {
                            LOG_ERROR("lost-track PushAlert 失败: {}",
                                      alert_res.error);
                        }

                        ImageData face_roi;
                        if (!CropFaceRoiFromFrame(ivps, frame, face_copy,
                                                  face_roi)) {
                            LOG_ERROR("lost-track crop face failed track_id={}",
                                      lost_track_id);
                            return;
                        }

                        std::vector<uint8_t> face_jpg;
                        if (!EncodeVisitorFaceJpeg(ivps, face_roi, face_jpg)) {
                            LOG_ERROR(
                                "lost-track face JpegEncode failed track_id={}",
                                lost_track_id);
                            return;
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
                if (!submitted) {
                    LOG_ERROR(
                        "丢轨陌生人推送丢弃: 线程池队列已满 track_id={}",
                        lost_track_id);
                }
            }

            // TIME_END(arcface);
            // TIME_USEC_SHOW(arcface);

            if (Map(in_data->image) != 0)
            {
                LOG_ERROR("BusProcess Map failed, skip draw");
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
        bool feat_done = false;
        int last_seen_frame = 0;
    };

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
    std::unordered_map<int, TrackPending> track_pending_;
    int frame_seq_ = 0;
    // 未见超过该帧数则 prune（约 2 * track_buffer）
    static constexpr int kTrackPruneFrames = 60;
    // 正脸加权分 Infer 门控阈值（与 FrontalScoreRecommended 配套）
    static constexpr float kFrontalScoreThresh = 0.55f;

    IvpsHelper* ivps_ = nullptr;
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
