#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "http_client.h"

namespace face_server {

using Json = nlohmann::json;

struct FaceServerConfig {
  std::string base_url = "http://127.0.0.1:8848";
  std::string alerts_api_key = "d16f6b2fe30b552b4744dbe0157d26a13530632ef430f66e";
  std::string visitors_api_key = "9e4c1a7b3f82d056e1c8a94b7d203f5e6a18c4b9d7e2f0a3";
  http::HttpConfig http;
};

struct ImageBlob {
  std::string filename;
  std::string content_type;  // image/jpeg | image/png
  std::vector<uint8_t> data;
  // const void* data = nullptr;
  // size_t size = 0;
};

struct AlertPushRequest {
  std::string msg_id;
  std::string event_id;
  std::string org_id;
  std::string bank_id;
  std::string event_time;  // YYYY-MM-DD HH:MM:SS or YYYYMMDD
  std::string event_name;
  std::string channel_name;
  std::string description;  // key required; value may be empty
  std::vector<ImageBlob> files;  // >=1, form field name "files"
};

struct VisitorPerson {
  std::string event_id;
  std::string msg_id;  // optional if top-level msg_id set
  bool recognized = false;
  std::string name;
  std::string ehr_no;
};

struct VisitorPushRequest {
  std::string event_time;   // YYYY-MM-DD HH:MM:SS
  std::string camera_name;
  std::optional<std::string> msg_id;  // optional top-level default
  std::vector<VisitorPerson> persons;
  ImageBlob original;                 // form field "original"
  std::vector<ImageBlob> faces;       // form field "faces", same order as persons
};

struct ApiResult {
  bool ok = false;  // transport 2xx and envelope code==0 (when present)
  long http_status = 0;
  int code = -1;  // face_server envelope; -1 if not parsed
  std::string message;
  std::string raw_body;
  Json body;
  std::string error;

  explicit operator bool() const { return ok; }
};

class FaceServerClient {
 public:
  explicit FaceServerClient(FaceServerConfig config);
  ~FaceServerClient();

  FaceServerClient(const FaceServerClient&) = delete;
  FaceServerClient& operator=(const FaceServerClient&) = delete;

  ApiResult PushAlert(const AlertPushRequest& req) const;
  ApiResult PushVisitor(const VisitorPushRequest& req) const;

 private:
  ApiResult FromHttp(const http::HttpResponse& http_res) const;
  std::string JoinUrl(const std::string& path) const;

  FaceServerConfig config_;
  std::unique_ptr<http::HttpClient> http_;
};

}  // namespace face_server
