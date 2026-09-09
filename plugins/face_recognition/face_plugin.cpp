#include "face_plugin.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "face_align.h"
#include "logger.h"
#include "my_utils.h"

FacePlugin::~FacePlugin()
{
    Shutdown();
}

const char* FacePlugin::Name() const
{
    return "face_recognition";
}

uint32_t FacePlugin::ApiVersion() const
{
    return kBusinessPluginApiVersion;
}

int FacePlugin::Init(HostServices* host, const PluginConfig& cfg)
{
    (void)cfg;
    if (host == nullptr)
    {
        LOG_ERROR("FacePlugin: host is null");
        return -1;
    }
    host_ = host;
    frontal_score_thresh_ =
        host_->ConfigFloat("frontal_score_thresh", 0.55f);
    collection_ = host_->ConfigString("qdrant_collection", "face_embeddings");
    return 0;
}

void FacePlugin::Shutdown()
{
    track_pending_.clear();
    std::lock_guard<std::mutex> lock(identify_results_mutex_);
    identify_results_.clear();
}

int FacePlugin::OnFrame(const ImageData& frame,
                        const std::vector<detection::Object>& objects)
{
    if (host_ == nullptr)
    {
        return -1;
    }

    const std::string capture_msg_id = my_utils::GenerateUuid();
    const std::string cur_time = my_utils::GetCurrentTimeIso8601Utc();
    const std::int64_t cur_created_at = my_utils::GetUnixMilliseconds();

    std::vector<std::vector<float>> feats;
    std::vector<ImageData> face_imgs;

    DrainIdentifyResults();

    std::vector<detection::Object> tracked_faces = objects;
    ++frame_seq_;
    const auto& frontal_score_cfg = face_align::FrontalScoreRecommended();
    auto tracks = face_tracker_.update(tracked_faces);

    auto rect_iou = [](const cv::Rect_<float>& a, float x1, float y1, float x2,
                       float y2) -> float {
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

    for (auto& f : tracked_faces)
    {
        f.track_id = -1;
    }
    std::vector<char> face_used(tracked_faces.size(), 0);
    int matched = 0;
    for (const auto& t : tracks)
    {
        if (t.tlbr.size() < 4)
        {
            continue;
        }
        int best_i = -1;
        float best_iou = 0.1f;
        for (size_t i = 0; i < tracked_faces.size(); ++i)
        {
            if (face_used[i])
            {
                continue;
            }
            const float iou = rect_iou(tracked_faces[i].rect, t.tlbr[0],
                                       t.tlbr[1], t.tlbr[2], t.tlbr[3]);
            if (iou > best_iou)
            {
                best_iou = iou;
                best_i = static_cast<int>(i);
            }
        }
        if (best_i >= 0)
        {
            tracked_faces[best_i].track_id = t.track_id;
            face_used[best_i] = 1;
            ++matched;
        }
    }

    LOG_INFO("bytetrack: faces={} tracks={} matched={}", tracked_faces.size(),
             tracks.size(), matched);

    for (const auto& t : tracks)
    {
        auto it = track_pending_.find(t.track_id);
        if (it != track_pending_.end())
        {
            it->second.last_seen_frame = frame_seq_;
        }
    }

    for (const auto& f : tracked_faces)
    {
        if (f.track_id < 0)
        {
            continue;
        }
        auto& pending = track_pending_[f.track_id];
        pending.last_seen_frame = frame_seq_;
        if (!pending.feat_done)
        {
            pending.last_face = f;
            if (host_->CloneFrame(pending.last_frame, frame) != 0)
            {
                LOG_ERROR("Clone last_frame for track {} failed", f.track_id);
                pending.last_frame = ImageData{};
            }
        }
    }

    std::vector<detection::Object> to_process;
    to_process.reserve(tracked_faces.size());
    for (const auto& f : tracked_faces)
    {
        if (f.track_id < 0)
        {
            continue;
        }
        auto& pending = track_pending_[f.track_id];
        if (pending.feat_done || pending.identify_inflight)
        {
            continue;
        }
        const float score =
            face_align::ComputeFrontalScore(f, frontal_score_cfg, nullptr);
        if (score < frontal_score_thresh_)
        {
            continue;
        }
        to_process.push_back(f);
    }

    host_->InferFaces(frame, to_process, face_imgs, feats);

    LOG_INFO("feats size {} (tracks {}/new {}/faces {} )", feats.size(),
             tracks.size(), to_process.size(), tracked_faces.size());

    std::vector<IdentifyItem> identify_batch;
    identify_batch.reserve(feats.size());
    for (size_t i = 0; i < feats.size(); ++i)
    {
        if (feats[i].empty())
        {
            if (i < to_process.size())
            {
                LOG_WARN("Infer 空特征 track_id={} score_gate={}",
                         to_process[i].track_id, frontal_score_thresh_);
            }
            continue;
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
        std::vector<int> inflight_ids;
        inflight_ids.reserve(identify_batch.size());
        for (const auto& item : identify_batch)
        {
            inflight_ids.push_back(item.track_id);
        }

        ImageData frame_copy;
        if (!host_->VectorsReady() || !host_->NotifyReady())
        {
            LOG_ERROR("检索依赖未就绪，跳过 count={}", inflight_ids.size());
            RollbackInflight(inflight_ids);
        }
        else if (host_->CloneFrame(frame_copy, frame) != 0 ||
                 frame_copy.data == nullptr)
        {
            LOG_ERROR("Clone frame for identify failed");
            RollbackInflight(inflight_ids);
        }
        else
        {
            const bool submitted = host_->SubmitAsync(
                Name(),
                [this, capture_msg_id, cur_time, cur_created_at,
                 batch = std::move(identify_batch),
                 copied = std::move(frame_copy)]() mutable {
                    RunIdentifyAndPush(std::move(batch), std::move(copied),
                                       capture_msg_id, cur_time,
                                       cur_created_at);
                });
            if (!submitted)
            {
                LOG_ERROR("检索推送丢弃: 线程池队列已满 count={}",
                          inflight_ids.size());
                RollbackInflight(inflight_ids);
            }
        }
    }

    for (auto it = track_pending_.begin(); it != track_pending_.end();)
    {
        if (frame_seq_ - it->second.last_seen_frame <= kTrackPruneFrames)
        {
            ++it;
            continue;
        }
        const int lost_track_id = it->first;
        if (it->second.feat_done)
        {
            it = track_pending_.erase(it);
            continue;
        }
        if (it->second.identify_inflight)
        {
            if (frame_seq_ - it->second.last_seen_frame >
                kTrackInflightMaxFrames)
            {
                LOG_ERROR("检索结果超时未回传，丢弃条目 track_id={}",
                          lost_track_id);
                it = track_pending_.erase(it);
            }
            else
            {
                ++it;
            }
            continue;
        }

        TrackPending pending = it->second;
        if (pending.last_frame.data == nullptr || !host_->NotifyReady())
        {
            it = track_pending_.erase(it);
            continue;
        }

        ImageData frame_copy;
        if (host_->CloneFrame(frame_copy, pending.last_frame) != 0 ||
            frame_copy.data == nullptr)
        {
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
        HostServices* host = host_;

        const bool submitted = host_->SubmitAsync(
            Name(),
            [host, lost_msg_id, lost_time, lost_track_id, alert_description,
             face_copy, copied = std::move(frame_copy)]() mutable {
                ImageData face_roi;
                if (!host->CropFaceRoi(copied, face_copy, face_roi))
                {
                    LOG_ERROR("lost-track crop face failed track_id={}",
                              lost_track_id);
                    return;
                }

                std::vector<uint8_t> face_jpg;
                if (!host->EncodeVisitorFace(face_roi, face_jpg))
                {
                    LOG_ERROR("lost-track face JpegEncode failed track_id={}",
                              lost_track_id);
                    return;
                }

                std::vector<uint8_t> frame_jpg;
                if (host->EncodeJpeg(frame_jpg, copied) != 0 ||
                    frame_jpg.empty())
                {
                    LOG_ERROR("lost-track JpegEncode frame failed");
                    return;
                }

                auto* client = host->Notify();
                if (client == nullptr)
                {
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
                if (!alert_res)
                {
                    LOG_ERROR("lost-track PushAlert 失败: {}", alert_res.error);
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
                if (!vis_res)
                {
                    LOG_ERROR("lost-track PushVisitor 失败: {}", vis_res.error);
                }
            });
        if (submitted)
        {
            it = track_pending_.erase(it);
        }
        else
        {
            LOG_ERROR("丢轨陌生人推送丢弃: 线程池队列已满 track_id={}",
                      lost_track_id);
            ++it;
        }
    }

    // 画当前帧人脸：有 track_id 用红框；未匹配到轨用黄框标 -1。
    // 默认关闭，保持与改造前行为一致。
    // for (const auto& item : tracked_faces)
    // {
    //     const bool tracked = item.track_id >= 0;
    //     const auto color = tracked ? YUVColors::kRed : YUVColors::kYellow;
    //     std::string txt =
    //         std::to_string(item.track_id) + " " + std::to_string(item.prob);
    //     host_->DrawText(frame, static_cast<int>(item.rect.x),
    //                     static_cast<int>(item.rect.y) + 5, txt, color, 32);
    //     host_->DrawRect(frame, static_cast<int>(item.rect.x),
    //                     static_cast<int>(item.rect.y),
    //                     static_cast<int>(item.rect.x + item.rect.width),
    //                     static_cast<int>(item.rect.y + item.rect.height),
    //                     color, 2);
    // }

    return 0;
}

void FacePlugin::PostIdentifyResult(int track_id, bool ok)
{
    std::lock_guard<std::mutex> lock(identify_results_mutex_);
    identify_results_.push_back({track_id, ok});
}

void FacePlugin::DrainIdentifyResults()
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
            continue;
        }
        it->second.identify_inflight = false;
        if (r.ok)
        {
            it->second.feat_done = true;
            it->second.last_frame = ImageData{};
        }
        else
        {
            LOG_WARN("检索失败，track_id={} 允许后续帧重试", r.track_id);
        }
    }
}

void FacePlugin::RollbackInflight(const std::vector<int>& track_ids)
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

std::string FacePlugin::PayloadString(const Json& payload, const char* key,
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

void FacePlugin::RunIdentifyAndPush(std::vector<IdentifyItem> batch,
                                    ImageData frame,
                                    const std::string& capture_msg_id,
                                    const std::string& cur_time,
                                    std::int64_t cur_created_at)
{
    if (host_ == nullptr)
    {
        return;
    }
    auto* client = host_->Vectors();
    auto* notify = host_->Notify();
    if (client == nullptr || notify == nullptr)
    {
        for (const auto& item : batch)
        {
            PostIdentifyResult(item.track_id, false);
        }
        return;
    }

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
        auto search_res =
            client->Search(collection_, item.feat, 1, {}, true, false, 0.65f);
        if (!search_res)
        {
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
        auto upsert_res = client->UpsertPoints(collection_, points);
        if (!upsert_res)
        {
            LOG_ERROR("写入点失败: {}", upsert_res.error);
        }
    }

    const bool need_visitor = !visitor_req.persons.empty() &&
                              visitor_faces.size() == visitor_req.persons.size();
    if (!visitor_req.persons.empty() && !need_visitor)
    {
        LOG_ERROR("visitor person and face not eq {}=={}", visitor_faces.size(),
                  visitor_req.persons.size());
    }
    if (!is_send_alert && !need_visitor)
    {
        return;
    }

    std::vector<uint8_t> frame_jpg;
    if (host_->EncodeJpeg(frame_jpg, frame) != 0 || frame_jpg.empty())
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

        auto alert_res = notify->PushAlert(alert_req);
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
            if (!host_->EncodeVisitorFace(visitor_faces[j], face_jpg))
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
            auto vis_res = notify->PushVisitor(visitor_req);
            if (!vis_res)
            {
                LOG_ERROR("PushVisitor 失败: {}", vis_res.error);
            }
        }
    }
}

Json FacePlugin::MakeStrangerBoxJson(const detection::Object& face)
{
    return Json{{"x", face.rect.x},
                {"y", face.rect.y},
                {"width", face.rect.width},
                {"height", face.rect.height}};
}

std::string FacePlugin::MakeStrangerAlertDescription(
    int track_id, const detection::Object& face)
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

std::string FacePlugin::MakeStrangerAlertDescription(
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

extern "C" {

__attribute__((visibility("default"))) BusinessPlugin* CreatePlugin()
{
    return new FacePlugin();
}

__attribute__((visibility("default"))) void DestroyPlugin(BusinessPlugin* p)
{
    delete p;
}

}  // extern "C"
