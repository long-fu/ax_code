#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "http_client.h"

namespace qdrant {

using Json = nlohmann::json;

// ------------------------- 配置 -------------------------

struct QdrantConfig {
  std::string host = "localhost";
  int port = 6333;
  bool use_https = false;
  std::string api_key;  // 为空则不携带 api-key 头
  long timeout_ms = 10000;
  long connect_timeout_ms = 5000;

  // 连接池:常驻 easy handle 的最大数量(即最大并发请求数)。
  size_t pool_size = 4;

  // 重试策略:仅对网络层失败(连接失败/超时)和 HTTP 429/5xx 生效。
  int max_retries = 2;
  long retry_backoff_ms = 200;

  std::string BaseUrl() const {
    return (use_https ? std::string("https://") : std::string("http://")) +
           host + ":" + std::to_string(port);
  }
};

// ------------------------- 数据结构 -------------------------

struct Point {
  std::string id;
  bool id_is_numeric = true;
  std::vector<float> vector;
  Json payload = Json::object();

  static Point WithNumericId(uint64_t id, std::vector<float> vector,
                             Json payload = Json::object()) {
    Point p;
    p.id = std::to_string(id);
    p.id_is_numeric = true;
    p.vector = std::move(vector);
    p.payload = std::move(payload);
    return p;
  }

  static Point WithStringId(std::string id, std::vector<float> vector,
                            Json payload = Json::object()) {
    Point p;
    p.id = std::move(id);
    p.id_is_numeric = false;
    p.vector = std::move(vector);
    p.payload = std::move(payload);
    return p;
  }
};

struct ApiResult {
  bool ok = false;
  long http_status = 0;
  std::string raw_body;
  Json body;
  std::string error;

  explicit operator bool() const { return ok; }
};

// One hit from /points/search (or compatible result array).
struct ScoredPoint {
  std::string id;
  bool id_is_numeric = true;
  float score = 0.f;
  Json payload = Json::object();
  std::vector<float> vector;  // empty unless with_vector=true
};

struct SearchResult : ApiResult {
  std::vector<ScoredPoint> points;
};

class QdrantException : public std::runtime_error {
 public:
  QdrantException(std::string message, ApiResult result)
      : std::runtime_error(std::move(message)), result_(std::move(result)) {}

  const ApiResult& Result() const { return result_; }

 private:
  ApiResult result_;
};

class QdrantClient {
 public:
  explicit QdrantClient(QdrantConfig config);
  ~QdrantClient();

  QdrantClient(const QdrantClient&) = delete;
  QdrantClient& operator=(const QdrantClient&) = delete;

  ApiResult CreateCollection(const std::string& collection, uint64_t vector_size,
                             const std::string& distance = "Cosine",
                             bool recreate_if_exists = false);

  ApiResult DeleteCollection(const std::string& collection);

  bool CollectionExists(const std::string& collection);

  ApiResult GetCollectionInfo(const std::string& collection);

  ApiResult ListCollections();

  ApiResult UpsertPoints(const std::string& collection,
                         const std::vector<Point>& points, bool wait = true);

  SearchResult Search(const std::string& collection,
                      const std::vector<float>& query_vector,
                      uint64_t limit = 10, const Json& filter = nullptr,
                      bool with_payload = true, bool with_vector = false,
                      std::optional<float> score_threshold = std::nullopt);

  ApiResult GetPoints(const std::string& collection,
                      const std::vector<std::string>& ids,
                      bool with_payload = true, bool with_vector = false);

  ApiResult DeletePoints(const std::string& collection,
                         const std::vector<std::string>& ids, bool wait = true);

  ApiResult DeletePointsByFilter(const std::string& collection,
                                 const Json& filter, bool wait = true);

  ApiResult Scroll(const std::string& collection, uint64_t limit = 10,
                   const Json& filter = nullptr, const std::string& offset = "",
                   bool with_payload = true, bool with_vector = false);

  ApiResult Count(const std::string& collection, const Json& filter = nullptr);

  bool Healthy();

 private:
  Json BuildPointsPayload(const std::vector<Point>& points) const;
  Json BuildIdArray(const std::vector<std::string>& ids) const;
  static std::vector<ScoredPoint> ParseScoredPoints(const Json& body);

  ApiResult Request(const std::string& method, const std::string& path,
                    const Json* body = nullptr) const;

  ApiResult DoRequest(const std::string& method, const std::string& path,
                      const Json* body) const;

  QdrantConfig config_;
  std::unique_ptr<http::HttpClient> http_;
};

}  // namespace qdrant
