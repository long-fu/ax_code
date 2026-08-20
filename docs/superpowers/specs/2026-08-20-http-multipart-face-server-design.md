# Http multipart + FaceServer push client

**Date:** 2026-08-20  
**Status:** Approved (conversation)

## Goal

1. Extend `http::HttpClient` with generic `multipart/form-data` upload (memory blobs only).
2. Add `face_server::FaceServerClient` for:
   - `POST /api/alerts/push` (multipart + `X-Api-Key`)
   - `POST /api/visitors/push` (multipart + `X-Api-Key`)

Out of scope: WebSocket, JSON-only push, BusProcess wiring, file-path uploads.

## HttpClient

```cpp
struct FormField { std::string name; std::string value; };

struct FormFile {
  std::string name;         // form field name
  std::string filename;
  std::string content_type; // e.g. image/jpeg
  const void* data = nullptr;
  size_t size = 0;
};

HttpResponse Upload(const std::string& url,
                    const std::vector<FormField>& fields,
                    const std::vector<FormFile>& files,
                    const std::vector<std::string>& headers = {}) const;
```

- Implement with `curl_mime` / `curl_mime_filedata` alternatives: `curl_mime_data` for memory.
- Reuse handle pool + timeouts from `Request`.
- Do not set `Content-Type` manually (libcurl sets boundary).

## FaceServerClient

Config: `base_url`, `alerts_api_key`, `visitors_api_key`, timeouts / pool via `HttpConfig`.

| Method | Endpoint |
|---|---|
| `PushAlert` | `/api/alerts/push` |
| `PushVisitor` | `/api/visitors/push` |

Alert form: text fields per `server-api.md` §6.4 A; files → repeated `files`.  
Visitor form: `eventTime`, `cameraName`, optional `msgId`, `persons` JSON string; files → one `original` + N `faces`.

Result: transport + HTTP status + raw body; parse JSON when possible; treat face_server envelope `code==0` as business OK when present.

## References

- `face_server/docs/api/server-api.md` §6.4 / §10.1
- WebSocket docs describe post-push broadcast only; HTTP multipart is the upload path.
