#include "http_client.h"

#include <curl/curl.h>

#include <condition_variable>
#include <mutex>
#include <vector>

namespace http {
namespace {

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

class CurlSList {
 public:
  void Append(const std::string& header) {
    list_ = curl_slist_append(list_, header.c_str());
  }
  curl_slist* Get() const { return list_; }
  ~CurlSList() {
    if (list_) {
      curl_slist_free_all(list_);
    }
  }

 private:
  curl_slist* list_ = nullptr;
};

}  // namespace

struct HttpClient::HandlePool {
  explicit HandlePool(size_t max_size)
      : max_size_(max_size == 0 ? 1 : max_size) {}

  ~HandlePool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (CURL* h : free_handles_) {
      curl_easy_cleanup(h);
    }
  }

  CURL* Acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!free_handles_.empty()) {
      CURL* h = free_handles_.back();
      free_handles_.pop_back();
      return h;
    }
    if (created_ < max_size_) {
      created_++;
      lock.unlock();
      return curl_easy_init();
    }
    cv_.wait(lock, [this] { return !free_handles_.empty(); });
    CURL* h = free_handles_.back();
    free_handles_.pop_back();
    return h;
  }

  void Release(CURL* h) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (h == nullptr) {
      created_--;
      return;
    }
    free_handles_.push_back(h);
    cv_.notify_one();
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<CURL*> free_handles_;
  size_t created_ = 0;
  size_t max_size_;
};

HttpClient::HttpClient(HttpConfig config)
    : config_(std::move(config)),
      pool_(std::make_unique<HandlePool>(config_.pool_size)) {}

HttpClient::~HttpClient() = default;

HttpResponse HttpClient::Request(const std::string& method,
                                 const std::string& url,
                                 const std::vector<std::string>& headers,
                                 const std::string* body) const {
  HttpResponse result;

  struct PooledHandle {
    HandlePool* pool;
    CURL* handle;
    explicit PooledHandle(HandlePool* p) : pool(p), handle(p->Acquire()) {}
    ~PooledHandle() { pool->Release(handle); }
    CURL* get() const { return handle; }
  };

  PooledHandle pooled(pool_.get());
  CURL* curl = pooled.get();
  if (!curl) {
    result.error = "curl_easy_init 失败";
    return result;
  }
  curl_easy_reset(curl);

  std::string response_body;
  std::string request_body;
  if (body != nullptr) {
    request_body = *body;
  }

  CurlSList slist;
  for (const auto& h : headers) {
    slist.Append(h);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist.Get());
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
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(request_body.size()));
  } else if (method == "PUT" || method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
  }

  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    result.error = std::string("curl 请求失败: ") + curl_easy_strerror(rc);
    return result;
  }

  long http_status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
  result.status = http_status;
  result.body = std::move(response_body);
  result.ok = (http_status >= 200 && http_status < 300);
  if (!result.ok) {
    result.error = "HTTP " + std::to_string(http_status);
  }
  return result;
}

HttpResponse HttpClient::Upload(const std::string& url,
                                const std::vector<FormField>& fields,
                                const std::vector<FormFile>& files,
                                const std::vector<std::string>& headers) const {
  HttpResponse result;

  struct PooledHandle {
    HandlePool* pool;
    CURL* handle;
    explicit PooledHandle(HandlePool* p) : pool(p), handle(p->Acquire()) {}
    ~PooledHandle() { pool->Release(handle); }
    CURL* get() const { return handle; }
  };

  PooledHandle pooled(pool_.get());
  CURL* curl = pooled.get();
  if (!curl) {
    result.error = "curl_easy_init 失败";
    return result;
  }
  curl_easy_reset(curl);

  curl_mime* mime = curl_mime_init(curl);
  if (!mime) {
    result.error = "curl_mime_init 失败";
    return result;
  }

  struct MimeGuard {
    curl_mime* m;
    ~MimeGuard() {
      if (m) {
        curl_mime_free(m);
      }
    }
  } mime_guard{mime};

  for (const auto& field : fields) {
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, field.name.c_str());
    curl_mime_data(part, field.value.c_str(), CURL_ZERO_TERMINATED);
  }

  for (const auto& file : files) {
    if (file.data == nullptr && file.size != 0) {
      result.error = "FormFile.data 为空但 size 非 0";
      return result;
    }
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, file.name.c_str());
    if (!file.filename.empty()) {
      curl_mime_filename(part, file.filename.c_str());
    }
    if (!file.content_type.empty()) {
      curl_mime_type(part, file.content_type.c_str());
    }
    curl_mime_data(part, static_cast<const char*>(file.data), file.size);
  }

  CurlSList slist;
  for (const auto& h : headers) {
    slist.Append(h);
  }

  std::string response_body;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist.Get());
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config_.connect_timeout_ms);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    result.error = std::string("curl 上传失败: ") + curl_easy_strerror(rc);
    return result;
  }

  long http_status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
  result.status = http_status;
  result.body = std::move(response_body);
  result.ok = (http_status >= 200 && http_status < 300);
  if (!result.ok) {
    result.error = "HTTP " + std::to_string(http_status);
  }
  return result;
}

}  // namespace http
