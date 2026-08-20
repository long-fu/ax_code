#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace http {

struct HttpConfig {
  long timeout_ms = 10000;
  long connect_timeout_ms = 5000;
  // Max concurrent CURL easy handles (connection reuse pool).
  size_t pool_size = 4;
};

struct HttpResponse {
  bool ok = false;  // transport succeeded and HTTP 2xx
  long status = 0;
  std::string body;
  std::string error;

  explicit operator bool() const { return ok; }
};

struct FormField {
  std::string name;
  std::string value;
};

// Memory-only multipart file part. Caller owns |data| for the duration of Upload().
struct FormFile {
  std::string name;          // form field name (e.g. files / original / faces)
  std::string filename;      // remote filename (e.g. a.jpg)
  std::string content_type;  // e.g. image/jpeg; empty => libcurl default
  const void* data = nullptr;
  size_t size = 0;
};

// Thin libcurl wrapper. Header does not include <curl/curl.h>.
class HttpClient {
 public:
  explicit HttpClient(HttpConfig config = {});
  ~HttpClient();

  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  // method: GET/POST/PUT/DELETE/...
  // body: nullptr = no body; empty string still sends Content-Length 0 for PUT/POST.
  HttpResponse Request(const std::string& method, const std::string& url,
                       const std::vector<std::string>& headers = {},
                       const std::string* body = nullptr) const;

  // multipart/form-data POST via curl_mime. Do not set Content-Type in headers.
  HttpResponse Upload(const std::string& url,
                      const std::vector<FormField>& fields,
                      const std::vector<FormFile>& files,
                      const std::vector<std::string>& headers = {}) const;

 private:
  struct HandlePool;

  HttpConfig config_;
  std::unique_ptr<HandlePool> pool_;
};

}  // namespace http
