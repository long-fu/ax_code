#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace qdrant {

using Json = nlohmann::json;

// ------------------------- 配置 -------------------------

struct QdrantConfig {
    std::string host = "localhost";
    int port = 6333;
    bool use_https = false;
    std::string api_key;          // 为空则不携带 api-key 头
    long timeout_ms = 10000;      // 单次请求超时时间
    long connect_timeout_ms = 5000;

    std::string BaseUrl() const {
        return (use_https ? std::string("https://") : std::string("http://")) +
               host + ":" + std::to_string(port);
    }
};

// ------------------------- 数据结构 -------------------------

// 向量点(用于 upsert)。payload 为可选的附加字段。
struct Point {
    // id 可以是无符号整数,也可以是字符串(UUID),这里用字符串统一表示,
    // 数字 id 直接传数字字符串即可,内部会自动按整数序列化。
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

// HTTP 请求结果,包含状态码、是否成功、原始/解析后的响应体
struct ApiResult {
    bool ok = false;
    long http_status = 0;
    std::string raw_body;
    Json body;             // 解析失败时为 nullptr
    std::string error;     // curl 层面或 HTTP 层面的错误描述

    explicit operator bool() const { return ok; }
};

// ------------------------- 异常 -------------------------

class QdrantException : public std::runtime_error {
public:
    QdrantException(std::string message, ApiResult result)
        : std::runtime_error(std::move(message)), result_(std::move(result)) {}

    const ApiResult& Result() const { return result_; }

private:
    ApiResult result_;
};

// ------------------------- 客户端 -------------------------

class QdrantClient {
public:
    explicit QdrantClient(QdrantConfig config);
    ~QdrantClient();

    QdrantClient(const QdrantClient&) = delete;
    QdrantClient& operator=(const QdrantClient&) = delete;

    // ---------- Collection ----------

    // distance: "Cosine" | "Euclid" | "Dot" | "Manhattan"
    ApiResult CreateCollection(const std::string& collection, uint64_t vector_size,
                                const std::string& distance = "Cosine",
                                bool recreate_if_exists = false);

    ApiResult DeleteCollection(const std::string& collection);

    bool CollectionExists(const std::string& collection);

    ApiResult GetCollectionInfo(const std::string& collection);

    ApiResult ListCollections();

    // ---------- Points ----------

    ApiResult UpsertPoints(const std::string& collection,
                            const std::vector<Point>& points,
                            bool wait = true);

    // filter 可传 nullptr / Json{} 表示不过滤
    ApiResult Search(const std::string& collection,
                      const std::vector<float>& query_vector,
                      uint64_t limit = 10,
                      const Json& filter = nullptr,
                      bool with_payload = true,
                      bool with_vector = false,
                      std::optional<float> score_threshold = std::nullopt);

    ApiResult GetPoints(const std::string& collection,
                         const std::vector<std::string>& ids,
                         bool with_payload = true,
                         bool with_vector = false);

    ApiResult DeletePoints(const std::string& collection,
                            const std::vector<std::string>& ids,
                            bool wait = true);

    ApiResult DeletePointsByFilter(const std::string& collection,
                                    const Json& filter,
                                    bool wait = true);

    ApiResult Scroll(const std::string& collection,
                      uint64_t limit = 10,
                      const Json& filter = nullptr,
                      const std::string& offset = "",
                      bool with_payload = true,
                      bool with_vector = false);

    ApiResult Count(const std::string& collection, const Json& filter = nullptr);

    // 健康检查:GET /
    bool Healthy();

private:
    Json BuildPointsPayload(const std::vector<Point>& points) const;
    Json BuildIdArray(const std::vector<std::string>& ids) const;

    ApiResult Request(const std::string& method, const std::string& path,
                       const Json* body = nullptr) const;

    QdrantConfig config_;
};

}  // namespace qdrant
