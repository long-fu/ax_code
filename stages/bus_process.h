#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
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
#include <vector>
#include "my_utils.h"
#include "thread_pool.h"
#include "face_server_client.h"

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

            TIME_START(arcface);

            const std::string capture_msg_id = my_utils::GenerateUuid();
            const std::string cur_time = my_utils::GetCurrentTimeYmdHMS();
            const int cur_created_at = static_cast<int>(my_utils::GetUnixSeconds());

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

            auto payload_int = [](const Json& payload, const char* key,
                                  int def) -> int {
                if (!payload.contains(key))
                {
                    return def;
                }
                const auto& v = payload[key];
                if (v.is_number_integer())
                {
                    return v.get<int>();
                }
                if (v.is_number_unsigned())
                {
                    return static_cast<int>(v.get<uint64_t>());
                }
                if (v.is_string())
                {
                    try
                    {
                        return std::stoi(v.get<std::string>());
                    }
                    catch (...)
                    {
                        return def;
                    }
                }
                return def;
            };

            // 过滤有效人脸
            std::vector<detection::Object> frontal_faces;
            frontal_faces.reserve(faces.size());
            for (const auto& f : faces)
            {
                face_align::FrontalMetrics metrics;
                if (face_align::IsFrontalFace(f, {}, &metrics))
                {
                    frontal_faces.push_back(f);
                }
                else
                {
                    const auto& lm = f.landmark;
                    LOG_INFO(
                        "skip non-frontal face: eye_dist={:.1f} roll={:.1f} "
                        "yaw={:.2f} pitch={:.2f} sym={:.2f} valid={} | "
                        "lm le=({:.1f},{:.1f}) re=({:.1f},{:.1f}) "
                        "nose=({:.1f},{:.1f}) lmouth=({:.1f},{:.1f}) "
                        "rmouth=({:.1f},{:.1f})",
                        metrics.eye_dist, metrics.roll_deg, metrics.yaw_proxy,
                        metrics.pitch_proxy, metrics.sym, metrics.valid,
                        lm[0].x, lm[0].y, lm[1].x, lm[1].y, lm[2].x, lm[2].y,
                        lm[3].x, lm[3].y, lm[4].x, lm[4].y);
                }
            }

            engine_->InferBatch(*ivps_, img_data, frontal_faces, face_imgs,
                                feats);
            LOG_INFO("feats size {} (frontal {}/{} )", feats.size(),
                     frontal_faces.size(), faces.size());

            for (size_t i = 0; i < feats.size(); ++i)
            {
                auto& feat = feats[i];
                if (feat.empty())
                {
                    continue;
                }
                // 可以加上对摄像头位置过滤
                auto search_res = client_->Search(collection_, feat, 1, {},
                                                  true, false, 0.85f);
                if (!search_res)
                {
                    LOG_ERROR("检索失败: {}", search_res.error);
                    continue;
                }

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

                const int created_at = payload_int(point.payload, "createdAt", 0);

                const int elapsed = cur_created_at - created_at;

                if (elapsed <= 60 * 5)
                {
                    // 5 分钟内同人已触发过，跳过
                    LOG_WARN("5 分钟内同人已触发过，跳过");
                    continue;
                }

                if (ehr_no == "NA")
                {
                    // 库中是陌生人记录，超时后再报
                    LOG_INFO("命中：陌生人");
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

            LOG_INFO("visitor person info {} , {}", visitor_faces.size(),
                     visitor_req.persons.size());

            const bool need_visitor = !visitor_req.persons.empty() && visitor_faces.size() == visitor_req.persons.size();

            if (!visitor_req.persons.empty() && !need_visitor)
            {
                LOG_ERROR("visitor person and face not eq {}=={}",
                          visitor_faces.size(), visitor_req.persons.size());
            }

            const bool need_push = (is_send_alert || need_visitor) && face_server_ && push_pool_;

            if (need_push)
            {
                LOG_INFO("需要推送");
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
                            
                            TIME_START(JpegEncode);

                            const int jpeg_ret = JpegEncode(frame_jpg, frame);
                            
                            TIME_END(JpegEncode);
                            TIME_USEC_SHOW(JpegEncode);

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
                                alert_req.event_id = my_utils::GenerateUuid();
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
                                    
                                    auto face_img = faces[j];

                                    TIME_START(BGR2YUV_JPEG);
                                    // faces 是BGR数据不支持转换
                                    AX_S32 ret = ivps->CropAndCSC(AX_FORMAT_YUV420_SEMIPLANAR,
                                                                static_cast<AX_U16>(0),
                                                                static_cast<AX_U16>(0),
                                                                static_cast<AX_U16>(face_img.width),
                                                                static_cast<AX_U16>(face_img.height));
                                    if (ret != 0) {
                                        LOG_ERROR("BGR2YUV: CropAndCSC failed, ret={}", ret);
                                        // return aligned;
                                        return;
                                    }

                                    ImageData yuv_frame;
                                    ret = ivps->Process(yuv_frame, face_img);
                                    if (ret != 0) {
                                        LOG_ERROR("BGR2YUV: IVPS Process failed, ret={}", ret);
                                        // return aligned;
                                        return;
                                    }

                                    const int enc_ret = JpegEncode(face_jpg, yuv_frame);
                                    
                                    if (enc_ret != 0 || face_jpg.empty())
                                    {
                                        LOG_ERROR(
                                            "face JpegEncode Failed j={} "
                                            "ret={:#x}",
                                            j, enc_ret);
                                        visitor_req.faces.clear();
                                        break;
                                    }
                                    // else
                                    // {
                                    //     LOG_INFO("JpegEncode SUCCESS");
                                    // }

                                    TIME_END(BGR2YUV_JPEG);
                                    TIME_USEC_SHOW(BGR2YUV_JPEG);
                                    
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

            TIME_END(arcface);
            TIME_USEC_SHOW(arcface);

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
                // for (size_t i = 0; i < in_data->objects.size(); i++)
                // {
                //     auto item = in_data->objects[i];
                //     auto* frame = in_data->image.data->FrameInfo();
                //     std::string txt = std::to_string(item.label) + " " + std::to_string(item.prob);
                //     DrawText(frame, item.rect.x, item.rect.y + 5, txt, YUVColors::kRed);
                //     DrawRect(frame,
                //              static_cast<int>(item.rect.x),
                //              static_cast<int>(item.rect.y),
                //              static_cast<int>(item.rect.x + item.rect.width),
                //              static_cast<int>(item.rect.y + item.rect.height),
                //              YUVColors::kRed, 2);

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
    // SortTracker tracker_;
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
