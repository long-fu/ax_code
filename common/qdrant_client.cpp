#include "qdrant_client.hpp"

#include <chrono>
#include <thread>

namespace qdrant {

QdrantClient::QdrantClient(QdrantConfig config) : config_(std::move(config)) {
  http::HttpConfig http_cfg;
  http_cfg.timeout_ms = config_.timeout_ms;
  http_cfg.connect_timeout_ms = config_.connect_timeout_ms;
  http_cfg.pool_size = config_.pool_size;
  http_ = std::make_unique<http::HttpClient>(http_cfg);
}

QdrantClient::~QdrantClient() = default;

// 带重试的入口:网络层失败或 429/5xx 才重试,指数退避;4xx 不重试。
ApiResult QdrantClient::Request(const std::string& method,
                                const std::string& path,
                                const Json* body) const {
  ApiResult result;
  int attempt = 0;
  while (true) {
    attempt++;
    result = DoRequest(method, path, body);

    bool retryable = false;
    if (!result.ok) {
      if (result.http_status == 0) {
        retryable = true;  // curl 层错误:连接失败、超时等
      } else if (result.http_status == 429 || result.http_status == 500 ||
                 result.http_status == 502 || result.http_status == 503 ||
                 result.http_status == 504) {
        retryable = true;  // 限流 / 服务端临时性错误
      }
    }

    if (result.ok || !retryable || attempt > config_.max_retries) {
      break;
    }

    long backoff_ms =
        config_.retry_backoff_ms * (1L << (attempt - 1));  // 指数退避
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
  }
  return result;
}

ApiResult QdrantClient::DoRequest(const std::string& method,
                                  const std::string& path,
                                  const Json* body) const {
  ApiResult result;

  std::vector<std::string> headers = {
      "Content-Type: application/json",
      "Accept: application/json",
  };
  if (!config_.api_key.empty()) {
    headers.push_back("api-key: " + config_.api_key);
  }

  std::string request_body;
  const std::string* body_ptr = nullptr;
  if (body != nullptr) {
    request_body = body->dump();
    body_ptr = &request_body;
  }

  const std::string url = config_.BaseUrl() + path;
  http::HttpResponse http_resp =
      http_->Request(method, url, headers, body_ptr);

  result.http_status = http_resp.status;
  result.raw_body = http_resp.body;
  result.error = http_resp.error;

  if (!http_resp.body.empty()) {
    try {
      result.body = Json::parse(http_resp.body);
    } catch (const std::exception& e) {
      result.body = nullptr;
      if (result.error.empty()) {
        result.error = std::string("响应体 JSON 解析失败: ") + e.what();
      }
    }
  }

  // Transport failure (status 0): keep http_resp.ok / error.
  if (http_resp.status == 0) {
    result.ok = false;
    return result;
  }

  result.ok = (http_resp.status >= 200 && http_resp.status < 300);
  if (!result.ok) {
    if (result.body.is_object() && result.body.contains("status")) {
      const auto& status = result.body["status"];
      if (status.is_object() && status.contains("error")) {
        result.error = status["error"].get<std::string>();
      } else if (status.is_string()) {
        result.error = status.get<std::string>();
      } else if (result.error.empty()) {
        result.error = "HTTP " + std::to_string(http_resp.status);
      }
    } else if (result.error.empty()) {
      result.error = "HTTP " + std::to_string(http_resp.status);
    }
  } else {
    result.error.clear();
  }

  return result;
}

// ------------------------- Collection -------------------------

ApiResult QdrantClient::CreateCollection(const std::string& collection,
                                         uint64_t vector_size,
                                         const std::string& distance,
                                         bool recreate_if_exists) {
  if (recreate_if_exists && CollectionExists(collection)) {
    DeleteCollection(collection);
  }

  Json body = {{"vectors", {{"size", vector_size}, {"distance", distance}}}};

  return Request("PUT", "/collections/" + collection, &body);
}

ApiResult QdrantClient::DeleteCollection(const std::string& collection) {
  return Request("DELETE", "/collections/" + collection);
}

bool QdrantClient::CollectionExists(const std::string& collection) {
  ApiResult r = Request("GET", "/collections/" + collection);
  return r.ok;
}

ApiResult QdrantClient::GetCollectionInfo(const std::string& collection) {
  return Request("GET", "/collections/" + collection);
}

ApiResult QdrantClient::ListCollections() { return Request("GET", "/collections"); }

// ------------------------- Points -------------------------

Json QdrantClient::BuildPointsPayload(const std::vector<Point>& points) const {
  Json arr = Json::array();
  for (const auto& p : points) {
    Json item;
    if (p.id_is_numeric) {
      item["id"] = std::stoull(p.id);
    } else {
      item["id"] = p.id;
    }
    item["vector"] = p.vector;
    item["payload"] = p.payload;
    arr.push_back(std::move(item));
  }
  return arr;
}

Json QdrantClient::BuildIdArray(const std::vector<std::string>& ids) const {
  Json arr = Json::array();
  for (const auto& id : ids) {
    try {
      size_t pos = 0;
      unsigned long long v = std::stoull(id, &pos);
      if (pos == id.size()) {
        arr.push_back(v);
        continue;
      }
    } catch (...) {
      // fallthrough to string
    }
    arr.push_back(id);
  }
  return arr;
}

ApiResult QdrantClient::UpsertPoints(const std::string& collection,
                                     const std::vector<Point>& points,
                                     bool wait) {
  Json body = {{"points", BuildPointsPayload(points)}};
  std::string path = "/collections/" + collection +
                     "/points?wait=" + (wait ? "true" : "false");
  return Request("PUT", path, &body);
}

ApiResult QdrantClient::Search(const std::string& collection,
                               const std::vector<float>& query_vector,
                               uint64_t limit, const Json& filter,
                               bool with_payload, bool with_vector,
                               std::optional<float> score_threshold) {
  Json body = {{"vector", query_vector},
               {"limit", limit},
               {"with_payload", with_payload},
               {"with_vector", with_vector}};
  if (!filter.is_null()) {
    body["filter"] = filter;
  }
  if (score_threshold.has_value()) {
    body["score_threshold"] = *score_threshold;
  }
  return Request("POST", "/collections/" + collection + "/points/search",
                 &body);
}

ApiResult QdrantClient::GetPoints(const std::string& collection,
                                  const std::vector<std::string>& ids,
                                  bool with_payload, bool with_vector) {
  Json body = {{"ids", BuildIdArray(ids)},
               {"with_payload", with_payload},
               {"with_vector", with_vector}};
  return Request("POST", "/collections/" + collection + "/points", &body);
}

ApiResult QdrantClient::DeletePoints(const std::string& collection,
                                     const std::vector<std::string>& ids,
                                     bool wait) {
  Json body = {{"points", BuildIdArray(ids)}};
  std::string path = "/collections/" + collection +
                     "/points/delete?wait=" + (wait ? "true" : "false");
  return Request("POST", path, &body);
}

ApiResult QdrantClient::DeletePointsByFilter(const std::string& collection,
                                             const Json& filter, bool wait) {
  Json body = {{"filter", filter}};
  std::string path = "/collections/" + collection +
                     "/points/delete?wait=" + (wait ? "true" : "false");
  return Request("POST", path, &body);
}

ApiResult QdrantClient::Scroll(const std::string& collection, uint64_t limit,
                               const Json& filter, const std::string& offset,
                               bool with_payload, bool with_vector) {
  Json body = {{"limit", limit},
               {"with_payload", with_payload},
               {"with_vector", with_vector}};
  if (!filter.is_null()) {
    body["filter"] = filter;
  }
  if (!offset.empty()) {
    body["offset"] = offset;
  }
  return Request("POST", "/collections/" + collection + "/points/scroll",
                 &body);
}

ApiResult QdrantClient::Count(const std::string& collection,
                              const Json& filter) {
  Json body = Json::object();
  if (!filter.is_null()) {
    body["filter"] = filter;
  }
  body["exact"] = true;
  return Request("POST", "/collections/" + collection + "/points/count", &body);
}

bool QdrantClient::Healthy() {
  ApiResult r = Request("GET", "/");
  return r.ok;
}

}  // namespace qdrant
