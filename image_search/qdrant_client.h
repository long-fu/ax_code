// /**
//  * qdrant_client.hpp
//  * A header-only C++ REST client for Qdrant 1.18.x
//  *
//  * Dependencies:
//  *   - libcurl        (HTTP transport)
//  *   - nlohmann/json  (JSON serialization)  https://github.com/nlohmann/json
//  * Usage:
//  *   QdrantClient client("http://localhost:6333");
//  *   // optional API key:
//  *   // QdrantClient client("http://localhost:6333", "your-api-key");
//  */

// #pragma once

// #include <string>
// #include <vector>
// #include <map>
// #include <optional>
// #include <stdexcept>
// #include <sstream>
// #include <curl/curl.h>
// #include <nlohmann/json.hpp>

// using json = nlohmann::json;

// // ─────────────────────────────────────────────
// //  Internal HTTP helper
// // ─────────────────────────────────────────────
// namespace qdrant_detail {

// inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
//     size_t total = size * nmemb;
//     out->append(static_cast<char*>(contents), total);
//     return total;
// }

// struct Response {
//     long        status_code{0};
//     std::string body;
// };

// inline Response http_request(const std::string& url,
//                               const std::string& method,
//                               const std::string& api_key,
//                               const std::string& payload = "") {
//     CURL* curl = curl_easy_init();
//     if (!curl) throw std::runtime_error("curl_easy_init() failed");

//     std::string response_body;
//     struct curl_slist* headers = nullptr;
//     headers = curl_slist_append(headers, "Content-Type: application/json");
//     if (!api_key.empty()) {
//         std::string auth_header = "api-key: " + api_key;
//         headers = curl_slist_append(headers, auth_header.c_str());
//     }

//     curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
//     curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
//     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
//     curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response_body);
//     curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);

//     if (method == "POST") {
//         curl_easy_setopt(curl, CURLOPT_POST, 1L);
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     payload.c_str());
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)payload.size());
//     } else if (method == "PUT") {
//         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    payload.c_str());
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
//     } else if (method == "DELETE") {
//         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
//     } else if (method == "PATCH") {
//         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    payload.c_str());
//         curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
//     }

//     CURLcode res = curl_easy_perform(curl);
//     if (res != CURLE_OK) {
//         curl_slist_free_all(headers);
//         curl_easy_cleanup(curl);
//         throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(res));
//     }

//     Response resp;
//     curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
//     resp.body = std::move(response_body);

//     curl_slist_free_all(headers);
//     curl_easy_cleanup(curl);
//     return resp;
// }

// inline json check(const Response& r, const std::string& op) {
//     json j = json::parse(r.body, nullptr, false);
//     if (j.is_discarded())
//         throw std::runtime_error(op + ": invalid JSON: " + r.body);
//     if (r.status_code < 200 || r.status_code >= 300)
//         throw std::runtime_error(op + " HTTP " + std::to_string(r.status_code) + ": " + r.body);
//     return j;
// }

// } // namespace qdrant_detail

// // ─────────────────────────────────────────────
// //  Data types
// // ─────────────────────────────────────────────

// struct VectorParams {
//     size_t      size;
//     std::string distance{"Cosine"};    // Cosine | Euclid | Dot | Manhattan
//     std::string vector_name{""};       // "" = unnamed (default), non-empty = named vector
// };

// struct PointStruct {
//     json                            id;       // uint64 or UUID string
//     std::vector<float>              vector;
//     std::map<std::string, json>     payload;
//     std::string                     vector_name{""};  // must match collection's vector_name
// };

// struct ScoredPoint {
//     json                              id;
//     float                             score{0.f};
//     std::map<std::string, json>       payload;
//     std::vector<float> vector;
// };

// // ─────────────────────────────────────────────
// //  Client
// // ─────────────────────────────────────────────

// class QdrantClient {
// public:
//     explicit QdrantClient(std::string base_url, std::string api_key = "")
//         : base_url_(std::move(base_url))
//         , api_key_(std::move(api_key))
//     {}

//     // ── Collections ──────────────────────────

//     /** List all collection names. */
//     std::vector<std::string> list_collections() {
//         auto r = get("/collections");
//         auto j = qdrant_detail::check(r, "list_collections");
//         std::vector<std::string> names;
//         for (auto& c : j["result"]["collections"])
//             names.push_back(c["name"]);
//         return names;
//     }

//     /**
//      * Create a collection.
//      *
//      * vector_name == ""   →  unnamed (default) vector
//      *                        {"vectors": {"size":N, "distance":"Cosine"}}
//      *
//      * vector_name != ""   →  named vector
//      *                        {"vectors": {"my_vec": {"size":N, "distance":"Cosine"}}}
//      *
//      * upsert_points() and query_points() read vector_name from
//      * PointStruct / the `using` parameter automatically.
//      */
//     void create_collection(const std::string& name, const VectorParams& params) {
//         json vec_cfg = {{"size", params.size}, {"distance", params.distance}};
//         json body;
//         if (params.vector_name.empty()) {
//             body["vectors"] = vec_cfg;
//         } else {
//             body["vectors"] = json::object({{params.vector_name, vec_cfg}});
//         }
//         auto r = put("/collections/" + name, body.dump());
//         qdrant_detail::check(r, "create_collection");
//     }

//     /** Delete a collection. */
//     void delete_collection(const std::string& name) {
//         auto r = del("/collections/" + name);
//         qdrant_detail::check(r, "delete_collection");
//     }

//     /** Get collection info. */
//     json get_collection(const std::string& name) {
//         auto r = get("/collections/" + name);
//         return qdrant_detail::check(r, "get_collection")["result"];
//     }

//     // ── Points ───────────────────────────────

//     /**
//      * Upsert a batch of points.
//      *
//      * p.vector_name == ""   →  {"vector": [...]}          (unnamed)
//      * p.vector_name != ""   →  {"vector": {"my_vec":[...]}} (named)
//      *
//      * All points in one call should use the same vector_name.
//      */
//     void upsert_points(const std::string& collection,
//                        const std::vector<PointStruct>& points,
//                        bool wait = true) {
//         json arr = json::array();
//         for (auto& p : points) {
//             json obj;
//             obj["id"] = p.id;
//             if (p.vector_name.empty()) {
//                 obj["vector"] = p.vector;
//             } else {
//                 obj["vector"] = json::object({{p.vector_name, p.vector}});
//             }
//             if (!p.payload.empty()) {
//                 json pl = json::object();
//                 for (auto& [k, v] : p.payload) pl[k] = v;
//                 obj["payload"] = pl;
//             }
//             arr.push_back(obj);
//         }
//         std::string url = "/collections/" + collection +
//                           "/points?wait=" + (wait ? "true" : "false");
//         auto r = put(url, json{{"points", arr}}.dump());
//         qdrant_detail::check(r, "upsert_points");
//     }

//     /** Retrieve a point by id. */
//     json get_point(const std::string& collection, const json& id) {
//         std::string sid = id.is_string() ? id.get<std::string>()
//                                          : std::to_string(id.get<uint64_t>());
//         auto r = get("/collections/" + collection + "/points/" + sid);
//         return qdrant_detail::check(r, "get_point")["result"];
//     }

//     /** Delete points by ids. */
//     void delete_points(const std::string& collection,
//                        const std::vector<json>& ids,
//                        bool wait = true) {
//         json body = {{"points", ids}};
//         std::string url = "/collections/" + collection +
//                           "/points/delete?wait=" + (wait ? "true" : "false");
//         auto r = post(url, body.dump());
//         qdrant_detail::check(r, "delete_points");
//     }

//     /** Set payload on existing points. */
//     void set_payload(const std::string& collection,
//                      const std::map<std::string, json>& payload,
//                      const std::vector<json>& ids,
//                      bool wait = true) {
//         json pl = json::object();
//         for (auto& [k, v] : payload) pl[k] = v;
//         json body = {{"payload", pl}, {"points", ids}};
//         std::string url = "/collections/" + collection +
//                           "/points/payload?wait=" + (wait ? "true" : "false");
//         auto r = post(url, body.dump());
//         qdrant_detail::check(r, "set_payload");
//     }

//     // ── Query / Search (v1.10+ unified API) ──

//     /**
//      * Vector similarity search.
//      *
//      * @param collection     collection name
//      * @param query_vector   query vector
//      * @param limit          max results (default 10)
//      * @param vector_name    "" = unnamed vector; non-empty → adds "using" field
//      * @param filter         optional Qdrant filter JSON
//      * @param with_payload   include payload in results
//      * @param score_threshold optional minimum score cutoff
//      */
//     std::vector<ScoredPoint> query_points(
//         const std::string&        collection,
//         const std::vector<float>& query_vector,
//         size_t                    limit            = 1,
//         const std::string&        vector_name      = "",
//         float      score_threshold  = 0,
//         json       filter           = {},
//         bool                      with_payload     = true)
//     {
//         json body = {
//             {"query",        query_vector},
//             {"limit",        limit},
//             {"with_payload", with_payload}
//         };
//         if (!vector_name.empty())  body["using"]           = vector_name;
//         // if (filter)                body["filter"]          = filter;
//         body["score_threshold"] = score_threshold;

//         auto r = post("/collections/" + collection + "/points/query", body.dump());
//         // print(r);
        
//         // fprintf(stdout,"post out: %s\n", r.body.c_str());

//         auto j = qdrant_detail::check(r, "query_points");

//         std::vector<ScoredPoint> results;
//         for (auto& pt : j["result"]["points"]) {
//             ScoredPoint sp;
//             sp.id    = pt["id"];
//             sp.score = pt.value("score", 0.f);
//             if (pt.contains("payload") && pt["payload"].is_object())
//                 for (auto& [k, v] : pt["payload"].items())
//                     sp.payload[k] = v;
//             if (pt.contains("vector") && pt["vector"].is_array())
//                 sp.vector = pt["vector"].get<std::vector<float>>();
//             results.push_back(std::move(sp));
//         }
//         return results;
//     }

//     /**
//      * Scroll through all points (no vector required).
//      */
//     json scroll_points(const std::string&  collection,
//                        json offset       = {},
//                        size_t              limit        = 10,
//                        json filter       = {},
//                        bool                with_payload = true) {
//         json body = {{"limit", limit}, {"with_payload", with_payload}};
//         if (offset) body["offset"] = offset;
//         if (filter) body["filter"] = filter;
//         auto r = post("/collections/" + collection + "/points/scroll", body.dump());
//         return qdrant_detail::check(r, "scroll_points")["result"];
//     }

//     /**
//      * Count points in a collection, optionally with a filter.
//      */
//     size_t count_points(const std::string&  collection,
//                         json filter = {}) {
//         json body = json::object();
//         if (filter) body["filter"] = filter;
//         auto r = post("/collections/" + collection + "/points/count", body.dump());
//         return qdrant_detail::check(r, "count_points")["result"]["count"].get<size_t>();
//     }

//     // ── Indexes ───────────────────────────────

//     /**
//      * Create a payload index.
//      * field_schema: "keyword" | "integer" | "float" | "bool" | "geo" | "text"
//      */
//     void create_payload_index(const std::string& collection,
//                               const std::string& field_name,
//                               const std::string& field_schema,
//                               bool wait = true) {
//         json body = {{"field_name", field_name}, {"field_schema", field_schema}};
//         std::string url = "/collections/" + collection +
//                           "/index?wait=" + (wait ? "true" : "false");
//         auto r = put(url, body.dump());
//         qdrant_detail::check(r, "create_payload_index");
//     }

//     // ── Service ───────────────────────────────

//     /** Health check. */
//     bool health_check() {
//         try {
//             auto r = get("/healthz");
//             return r.status_code == 200;
//         } catch (...) { return false; }
//     }

//     /** Telemetry / version info. */
//     json telemetry() {
//         auto r = get("/telemetry");
//         return qdrant_detail::check(r, "telemetry")["result"];
//     }

// private:
//     std::string base_url_;
//     std::string api_key_;

//     qdrant_detail::Response get(const std::string& path) {
//         return qdrant_detail::http_request(base_url_ + path, "GET", api_key_);
//     }
//     qdrant_detail::Response put(const std::string& path, const std::string& body) {
//         return qdrant_detail::http_request(base_url_ + path, "PUT", api_key_, body);
//     }
//     qdrant_detail::Response post(const std::string& path, const std::string& body) {
//         return qdrant_detail::http_request(base_url_ + path, "POST", api_key_, body);
//     }
//     qdrant_detail::Response del(const std::string& path) {
//         return qdrant_detail::http_request(base_url_ + path, "DELETE", api_key_);
//     }
// };