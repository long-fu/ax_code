#include "face_server_client.h"

#include <utility>

namespace face_server {
namespace {

bool LooksLikeJpeg(const void* data, size_t size) {
  if (data == nullptr || size < 3) {
    return false;
  }
  const auto* p = static_cast<const unsigned char*>(data);
  return p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF;
}

bool LooksLikePng(const void* data, size_t size) {
  static const unsigned char kSig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (data == nullptr || size < sizeof(kSig)) {
    return false;
  }
  const auto* p = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < sizeof(kSig); ++i) {
    if (p[i] != kSig[i]) {
      return false;
    }
  }
  return true;
}

std::string InferContentType(const ImageBlob& img) {
  if (!img.content_type.empty()) {
    return img.content_type;
  }
  if (LooksLikePng(img.data.data(), img.data.size())) {
    return "image/png";
  }
  if (LooksLikeJpeg(img.data.data(), img.data.size())) {
    return "image/jpeg";
  }
  return "application/octet-stream";
}

http::FormFile ToFormFile(const std::string& field_name, const ImageBlob& img) {
  http::FormFile f;
  f.name = field_name;
  f.filename = img.filename.empty() ? "upload.bin" : img.filename;
  f.content_type = InferContentType(img);
  f.data = img.data.data();
  f.size = img.data.size();
  return f;
}

}  // namespace

FaceServerClient::FaceServerClient(FaceServerConfig config)
    : config_(std::move(config)),
      http_(std::make_unique<http::HttpClient>(config_.http)) {}

FaceServerClient::~FaceServerClient() = default;

std::string FaceServerClient::JoinUrl(const std::string& path) const {
  std::string base = config_.base_url;
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (!path.empty() && path.front() == '/') {
    return base + path;
  }
  return base + "/" + path;
}

ApiResult FaceServerClient::FromHttp(const http::HttpResponse& http_res) const {
  ApiResult r;
  r.http_status = http_res.status;
  r.raw_body = http_res.body;

  if (http_res.status == 0 && !http_res.ok) {
    r.error = http_res.error.empty() ? "HTTP 传输失败" : http_res.error;
    return r;
  }

  try {
    if (!http_res.body.empty()) {
      r.body = Json::parse(http_res.body);
      if (r.body.contains("code") && r.body["code"].is_number_integer()) {
        r.code = r.body["code"].get<int>();
      }
      if (r.body.contains("message") && r.body["message"].is_string()) {
        r.message = r.body["message"].get<std::string>();
      }
    }
  } catch (const std::exception& e) {
    r.error = std::string("响应 JSON 解析失败: ") + e.what();
    if (!(http_res.status >= 200 && http_res.status < 300)) {
      if (r.error.empty()) {
        r.error = http_res.error;
      }
    }
    return r;
  }

  const bool http_ok = (http_res.status >= 200 && http_res.status < 300);
  if (!http_ok) {
    r.error = r.message.empty()
                  ? (http_res.error.empty()
                         ? ("HTTP " + std::to_string(http_res.status))
                         : http_res.error)
                  : r.message;
    return r;
  }

  if (r.code >= 0) {
    r.ok = (r.code == 0);
    if (!r.ok) {
      r.error = r.message.empty() ? ("业务 code=" + std::to_string(r.code))
                                  : r.message;
    }
    return r;
  }

  // No envelope code: fall back to HTTP 2xx.
  r.ok = true;
  return r;
}

ApiResult FaceServerClient::PushAlert(const AlertPushRequest& req) const {
  ApiResult bad;
  if (config_.alerts_api_key.empty()) {
    bad.error = "alerts_api_key 未配置";
    return bad;
  }
  if (req.files.empty()) {
    bad.error = "PushAlert 需要至少 1 张图片";
    return bad;
  }
  for (size_t i = 0; i < req.files.size(); ++i) {
    if (req.files[i].data.empty() || req.files[i].data.size() == 0) {
      bad.error = "PushAlert files[" + std::to_string(i) + "] 为空";
      return bad;
    }
  }

  std::vector<http::FormField> fields = {
      {"msgId", req.msg_id},
      {"eventId", req.event_id},
      {"orgId", req.org_id},
      {"bankId", req.bank_id},
      {"eventTime", req.event_time},
      {"eventName", req.event_name},
      {"channelName", req.channel_name},
      {"description", req.description},
  };

  std::vector<http::FormFile> files;
  files.reserve(req.files.size());
  for (size_t i = 0; i < req.files.size(); ++i) {
    ImageBlob img = req.files[i];
    if (img.filename.empty()) {
      img.filename = "alert_" + std::to_string(i) + ".jpg";
    }
    files.push_back(ToFormFile("files", img));
  }

  const std::vector<std::string> headers = {
      "X-Api-Key: " + config_.alerts_api_key,
  };

  auto http_res =
      http_->Upload(JoinUrl("/api/alerts/push"), fields, files, headers);
  return FromHttp(http_res);
}

ApiResult FaceServerClient::PushVisitor(const VisitorPushRequest& req) const {
  ApiResult bad;
  if (config_.visitors_api_key.empty()) {
    bad.error = "visitors_api_key 未配置";
    return bad;
  }
  if (req.persons.empty()) {
    bad.error = "PushVisitor 需要至少 1 个 person";
    return bad;
  }
  if (req.original.data.empty() || req.original.data.size() == 0) {
    bad.error = "PushVisitor original 为空";
    return bad;
  }
  if (req.faces.size() != req.persons.size()) {
    bad.error = "PushVisitor faces 数量须与 persons 一致";
    return bad;
  }
  for (size_t i = 0; i < req.faces.size(); ++i) {
    if (req.faces[i].data.empty() || req.faces[i].data.size() == 0) {
      bad.error = "PushVisitor faces[" + std::to_string(i) + "] 为空";
      return bad;
    }
  }

  Json persons = Json::array();
  for (const auto& p : req.persons) {
    Json item = {
        {"eventId", p.event_id},
        {"recognized", p.recognized},
    };
    if (!p.msg_id.empty()) {
      item["msgId"] = p.msg_id;
    }
    if (p.recognized) {
      item["name"] = p.name;
      item["ehrNo"] = p.ehr_no;
    }
    persons.push_back(std::move(item));
  }

  std::vector<http::FormField> fields = {
      {"eventTime", req.event_time},
      {"cameraName", req.camera_name},
      {"persons", persons.dump()},
  };
  if (req.msg_id.has_value() && !req.msg_id->empty()) {
    fields.push_back({"msgId", *req.msg_id});
  }

  std::vector<http::FormFile> files;
  {
    ImageBlob orig = req.original;
    if (orig.filename.empty()) {
      orig.filename = "original.jpg";
    }
    files.push_back(ToFormFile("original", orig));
  }
  for (size_t i = 0; i < req.faces.size(); ++i) {
    ImageBlob face = req.faces[i];
    if (face.filename.empty()) {
      face.filename = "face_" + std::to_string(i) + ".jpg";
    }
    files.push_back(ToFormFile("faces", face));
  }

  const std::vector<std::string> headers = {
      "X-Api-Key: " + config_.visitors_api_key,
  };

  auto http_res =
      http_->Upload(JoinUrl("/api/visitors/push"), fields, files, headers);
  return FromHttp(http_res);
}

}  // namespace face_server
