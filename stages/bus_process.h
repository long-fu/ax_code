#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
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
#include "qdrant_client.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include "utils.h"
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
        delete ivps_;
    };

    int Init() override
    {
        next_thread_id_ = pipeline::TaskNodeIdByName("EncProcess");

        qdrant::QdrantConfig config;
        config.host = "localhost";
        config.port = 6333;
        // config.api_key = "your-api-key"; // 若 Qdrant 开启了鉴权

        client_ = std::make_unique<qdrant::QdrantClient>(config);

        if (!client_->Healthy())
        {
            std::cerr << "无法连接到 Qdrant,请检查服务是否启动\n";
            return 1;
        }

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

        // // Load rule engine configuration
        // std::ifstream config_file("config.yaml");
        // if (config_file.is_open()) {
        //   std::stringstream buf;
        //   buf << config_file.rdbuf();
        //   std::string yaml_content = buf.str();

        //   // Extract rules section
        //   size_t pos = yaml_content.find("rules:");
        //   if (pos != std::string::npos) {
        //     size_t first_item = yaml_content.find("- ", pos);
        //     if (first_item != std::string::npos) {
        //       std::string rules_yaml = yaml_content.substr(first_item);
        //       RuleEngine::Instance().Load(rules_yaml);
        //     }
        //   }
        // }

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

            std::string msg_id = my_utils::GenerateUuid();
            std::string cur_time = my_utils::GetCurrentTimeYmdHMS();
            int cur_createdAt = my_utils::GetUnixSeconds();

            auto img_data = in_data->image;
            auto faces = in_data->objects;

            std::vector<std::vector<float> > feats;
            std::vector<ImageData> face_imgs;

            face_server::VisitorPushRequest visitor_person;

            // 检测到的人脸
            std::vector<ImageData> visitor_faces;

            visitor_person.msg_id = msg_id;
            visitor_person.event_time = cur_time;
            visitor_person.camera_name = "test_camera";
            

            bool isSendAlert = false;

            std::vector<qdrant::Point> points;

            engine_->InferBatch(*ivps_, img_data, faces, face_imgs, feats);

            LOG_INFO("feats size {}", feats.size());

            for (int i = 0; i < feats.size(); i++)
            {
                auto& feat = feats.at(i);

                auto search_res = client_->Search(collection_, feat,
                                                  1, {},
                                                  true, false, 0.85);
                if (!search_res)
                {
                    LOG_ERROR("检索失败: {}", search_res.error);
                }
                else
                {
                    const auto& hit = search_res.points.front();

                    LOG_INFO("检索命中 id={} score={}", hit.id, hit.score);

                    visitor_faces.push_back(face_imgs[i]);

                    if (search_res.points.empty())
                    {
                        isSendAlert = true;

                        // 数据库中没有 是陌生人

                        auto uuid = my_utils::GenerateUuid();
                        Json payload = {
                            {"createdAt", cur_createdAt},
                            {"faceId", "NA"},
                            {"name", "NA"},
                            {"ehrNo", "NA"}};
                        points.push_back(qdrant::Point::WithStringId(uuid, feat, payload));

                        face_server::VisitorPerson person;
                        person.ehr_no = "NA";
                        person.name = "NA";
                        person.recognized = false;
                        person.msg_id = msg_id;
                        person.event_id = "stranger";
                        visitor_person.persons.push_back(person);
                    }
                    else
                    {
                        // 数据库中存在

                        // TODO:
                        // 判断时间 是否重复

                        // 判断是否存在ERH号，存在是考勤，不存在是陌生人

                        auto point = search_res.points.at(0);
                        std::string ehrNo = point.payload["ehrNo"].get<std::string>();
                        std::string name = point.payload["name"].get<std::string>();
                        int createdAt = point.payload["createdAt"].get<int>();
                        int tmp = cur_createdAt - createdAt;

                        if (tmp > 60 * 5)
                        {
                            if (ehrNo == "NA")
                            {
                                // 数据库中记录是陌生人
                                isSendAlert = true;
                                // TODO: 这个人需要删除 不进行删除
                                // TODO: 需要新增吗 YES

                                // 新建陌生人
                                auto uuid = my_utils::GenerateUuid();
                                Json payload = {
                                    {"id", uuid},
                                    {"createdAt", cur_createdAt},
                                    {"ehrNo", "NA"}};
                                points.push_back(qdrant::Point::WithStringId(uuid, feat, payload));

                                // 陌生人记录
                                face_server::VisitorPerson person;
                                person.ehr_no = "NA";
                                person.name = "NA";
                                person.recognized = false;
                                person.msg_id = msg_id;
                                person.event_id = "stranger";
                                visitor_person.persons.push_back(person);
                            }
                            else
                            {
                                face_server::VisitorPerson person;
                                person.ehr_no = ehrNo;
                                person.name = name;
                                person.recognized = true;
                                person.msg_id = msg_id;
                                person.event_id = "visitor";
                                visitor_person.persons.push_back(person);

                            } // if (ehrNo == "NA")
                        }
                        else
                        {
                            // 5分钟之类同一个人 触发过
                        }
                    } // if (search_res.points.empty())
                } // if (!search_res)
            } //  for (int i = 0; i < feats.size(); i++)

            if (!points.empty())
            {
                // 陌生人的特征数据上传

                auto upsert_res = client_->UpsertPoints(collection_, points);
                if (!upsert_res)
                {
                    // std::cerr << "写入点失败: " << upsert_res.error << "\n";
                    LOG_ERROR("写入点失败: {}", upsert_res.error);
                    // return 1;
                }
                else
                {
                    // std::cout << "写入点成功\n";
                }
            }

            // 发送编码逻辑都切换到另外的线程

                       
            std::vector<uint8_t> img_jpg;
            int tmp_ret = JpegEncode(img_jpg, img_data);
            if (tmp_ret != 0)
            {
                LOG_ERROR("JpegEncode Failed ");
            }
            face_server::ImageBlob ori_img;
            ori_img.content_type = "image/jpeg";
            ori_img.filename = msg_id + ".jpeg";            
            ori_img.data = img_jpg; 
            // 发送预警
            if (isSendAlert)
            {

                face_server::AlertPushRequest alert_person;
                alert_person.bank_id = "test_bank_id";
                alert_person.msg_id = msg_id;
                alert_person.event_id = "test_event_id";
                alert_person.org_id = "test_org_id";
                alert_person.event_time = cur_time; // YYYY-MM-DD HH:MM:SS or YYYYMMDD
                alert_person.event_name = "陌生人闯入";
                alert_person.channel_name = "test_channel_name";
                alert_person.description = "陌生人闯入"; // key required; value may be empty



                std::vector<face_server::ImageBlob> files = {ori_img}; // >=1, form field name "files"

            }

            // visitor_person.persons
            LOG_INFO("visitor person info {} , {}", visitor_faces.size(),
                     visitor_person.persons.size());

            if (visitor_faces.size() == visitor_person.persons.size())
            {
                
                if (!visitor_faces.empty())
                {

                    visitor_person.msg_id = msg_id;
                    visitor_person.original = ori_img;

                    std::vector<uint8_t> img_jpg;
                    for (size_t j = 0; j < visitor_faces.size(); j++)
                    {

                        img_jpg.clear();
                        int tmp_ret = JpegEncode(img_jpg, visitor_faces.at(j));

                        if (tmp_ret != 0)
                        {
                            LOG_ERROR(" JpegEncode Failed ");
                        }
                        face_server::ImageBlob img;
                        img.content_type = "image/jpeg";
                        img.filename = msg_id + "_" + std::to_string(j) + ".jpeg";
                        img.data = img_jpg;
                        visitor_person.faces[j] = img;
                    }
                }
            }
            else
            {
                LOG_ERROR("visitor person  and face not eq {}=={}", visitor_faces.size(),
                          visitor_person.persons.size());
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
                // LOG_INFO("BusProcess draw: width={} height={} format={} size={}",
                //          in_data->image.width, in_data->image.height,
                //          (int)in_data->image.img_format,
                //          (int)in_data->objects.size());
                for (size_t i = 0; i < in_data->objects.size(); i++)
                {
                    auto item = in_data->objects[i];
                    std::string txt = std::to_string(item.label) + " " + std::to_string(item.prob);
                    DrawText(in_data->image.data->FrameInfo(), item.rect.x, item.rect.y + 5, txt, YUVColors::kRed);
                    DrawRect(in_data->image.data->FrameInfo(),
                             static_cast<int>(item.rect.x),
                             static_cast<int>(item.rect.y),
                             static_cast<int>(item.rect.x + item.rect.width),
                             static_cast<int>(item.rect.y + item.rect.height),
                             YUVColors::kRed, 2);
                    // LOG_INFO("BusProcess draw: label={} prob={} rect={} {} {} {}",
                    //  item.label, item.prob, item.rect.x, item.rect.y,
                    //  item.rect.width, item.rect.height);
                }
                Unmap(in_data->image);
            }

            // TIME_END(test_draw);
            // TIME_USEC_SHOW(test_draw);
            // Func test_draw cost : 663 us

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
};
