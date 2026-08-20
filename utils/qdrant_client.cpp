#include "qdrant_client.hpp"

#include <curl/curl.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

namespace qdrant {

namespace {

// libcurl 全局初始化只需要做一次(线程不安全,建议在 main 开始时或本文件加载时调用一次)
struct CurlGlobalInit {
    CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInit() { curl_global_cleanup(); }
};
CurlGlobalInit g_curl_global_init;

size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// RAII 包装 curl_slist,避免手动释放遗漏
class CurlSList {
public:
    void Append(const std::string& header) {
        list_ = curl_slist_append(list_, header.c_str());
    }
    curl_slist* Get() const { return list_; }
    ~CurlSList() {
        if (list_) curl_slist_free_all(list_);
    }

private:
    curl_slist* list_ = nullptr;
};

}  // namespace

QdrantClient::QdrantClient(QdrantConfig config) : config_(std::move(config)) {}

QdrantClient::~QdrantClient() = default;

ApiResult QdrantClient::Request(const std::string& method, const std::string& path,
                                 const Json* body) const {
    ApiResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.ok = false;
        result.error = "curl_easy_init 失败";
        return result;
    }

    std::string url = config_.BaseUrl() + path;
    std::string response_body;
    std::string request_body;

    CurlSList headers;
    headers.Append("Content-Type: application/json");
    headers.Append("Accept: application/json");
    if (!config_.api_key.empty()) {
        headers.Append("api-key: " + config_.api_key);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.Get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config_.connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }

    if (body != nullptr) {
        request_body = body->dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.size()));
    } else if (method == "PUT" || method == "POST") {
        // Qdrant 的部分 PUT/POST 接口(如不带 body 的 recreate)也需要显式空 body
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    }

    CURLcode rc = curl_easy_perform(curl);

    if (rc != CURLE_OK) {
        result.ok = false;
        result.error = std::string("curl 请求失败: ") + curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        return result;
    }

    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    curl_easy_cleanup(curl);

    result.http_status = http_status;
    result.raw_body = response_body;

    if (!response_body.empty()) {
        try {
            result.body = Json::parse(response_body);
        } catch (const std::exception& e) {
            result.body = nullptr;
            result.error = std::string("响应体 JSON 解析失败: ") + e.what();
        }
    }

    result.ok = (http_status >= 200 && http_status < 300);
    if (!result.ok && result.error.empty()) {
        if (result.body.is_object() && result.body.contains("status")) {
            // Qdrant 错误体一般形如 {"status": {"error": "..."}, "time": ...}
            const auto& status = result.body["status"];
            if (status.is_object() && status.contains("error")) {
                result.error = status["error"].get<std::string>();
            } else if (status.is_string()) {
                result.error = status.get<std::string>();
            } else {
                result.error = "HTTP " + std::to_string(http_status);
            }
        } else {
            result.error = "HTTP " + std::to_string(http_status);
        }
    }

    return result;
}

// ------------------------- Collection -------------------------

ApiResult QdrantClient::CreateCollection(const std::string& collection, uint64_t vector_size,
                                          const std::string& distance,
                                          bool recreate_if_exists) {
    if (recreate_if_exists && CollectionExists(collection)) {
        DeleteCollection(collection);
    }

    Json body = {
        {"vectors", {
            {"size", vector_size},
            {"distance", distance}
        }}
    };

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

ApiResult QdrantClient::ListCollections() {
    return Request("GET", "/collections");
}

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
    // 尝试把每个 id 解析为无符号整数,失败则当作字符串(UUID)处理
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
                                      const std::vector<Point>& points, bool wait) {
    Json body = {{"points", BuildPointsPayload(points)}};
    std::string path = "/collections/" + collection + "/points?wait=" +
                        (wait ? "true" : "false");
    return Request("PUT", path, &body);
}

ApiResult QdrantClient::Search(const std::string& collection,
                                const std::vector<float>& query_vector, uint64_t limit,
                                const Json& filter, bool with_payload, bool with_vector,
                                std::optional<float> score_threshold) {
    Json body = {
        {"vector", query_vector},
        {"limit", limit},
        {"with_payload", with_payload},
        {"with_vector", with_vector}
    };
    if (!filter.is_null()) {
        body["filter"] = filter;
    }
    if (score_threshold.has_value()) {
        body["score_threshold"] = *score_threshold;
    }
    return Request("POST", "/collections/" + collection + "/points/search", &body);
}

ApiResult QdrantClient::GetPoints(const std::string& collection,
                                   const std::vector<std::string>& ids, bool with_payload,
                                   bool with_vector) {
    Json body = {
        {"ids", BuildIdArray(ids)},
        {"with_payload", with_payload},
        {"with_vector", with_vector}
    };
    return Request("POST", "/collections/" + collection + "/points", &body);
}

ApiResult QdrantClient::DeletePoints(const std::string& collection,
                                      const std::vector<std::string>& ids, bool wait) {
    Json body = {{"points", BuildIdArray(ids)}};
    std::string path = "/collections/" + collection + "/points/delete?wait=" +
                        (wait ? "true" : "false");
    return Request("POST", path, &body);
}

ApiResult QdrantClient::DeletePointsByFilter(const std::string& collection,
                                              const Json& filter, bool wait) {
    Json body = {{"filter", filter}};
    std::string path = "/collections/" + collection + "/points/delete?wait=" +
                        (wait ? "true" : "false");
    return Request("POST", path, &body);
}

ApiResult QdrantClient::Scroll(const std::string& collection, uint64_t limit,
                                const Json& filter, const std::string& offset,
                                bool with_payload, bool with_vector) {
    Json body = {
        {"limit", limit},
        {"with_payload", with_payload},
        {"with_vector", with_vector}
    };
    if (!filter.is_null()) {
        body["filter"] = filter;
    }
    if (!offset.empty()) {
        body["offset"] = offset;
    }
    return Request("POST", "/collections/" + collection + "/points/scroll", &body);
}

ApiResult QdrantClient::Count(const std::string& collection, const Json& filter) {
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
