# Qdrant C++ Client (libcurl + nlohmann/json)

基于 `libcurl` 封装的 Qdrant REST API 客户端,C++17。

## 依赖

- libcurl (`libcurl4-openssl-dev` / `curl` via vcpkg / brew install curl)
- nlohmann/json (找不到会自动 FetchContent 拉取,也可提前 `apt install nlohmann-json3-dev`)
- CMake >= 3.14

## 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./qdrant_example
```

## 目录结构

```
include/qdrant_client.hpp   # 客户端接口声明
src/qdrant_client.cpp       # 基于 libcurl 的实现
example/main.cpp            # 使用示例
CMakeLists.txt
```

## 用法概览

```cpp
#include "qdrant_client.hpp"
using namespace qdrant;

QdrantConfig config;
config.host = "localhost";
config.port = 6333;
config.api_key = "";      // 若开启鉴权则填写

QdrantClient client(config);

// 创建 collection
client.CreateCollection("my_collection", /*vector_size=*/128, "Cosine");

// 写入点
std::vector<Point> points = {
    Point::WithNumericId(1, {0.1f, 0.2f, /* ... */}, {{"key", "value"}})
};
client.UpsertPoints("my_collection", points);

// 检索
auto res = client.Search("my_collection", query_vector, /*limit=*/10);
if (res) {
    std::cout << res.body.dump(2) << std::endl;
} else {
    std::cerr << "error: " << res.error << std::endl;
}
```

## 已覆盖的接口

| 方法 | 对应 Qdrant REST API |
|---|---|
| `CreateCollection` | `PUT /collections/{name}` |
| `DeleteCollection` | `DELETE /collections/{name}` |
| `CollectionExists` / `GetCollectionInfo` | `GET /collections/{name}` |
| `ListCollections` | `GET /collections` |
| `UpsertPoints` | `PUT /collections/{name}/points` |
| `Search` | `POST /collections/{name}/points/search` |
| `GetPoints` | `POST /collections/{name}/points` |
| `DeletePoints` / `DeletePointsByFilter` | `POST /collections/{name}/points/delete` |
| `Scroll` | `POST /collections/{name}/points/scroll` |
| `Count` | `POST /collections/{name}/points/count` |
| `Healthy` | `GET /` |

## 说明

- 所有请求方法统一返回 `ApiResult`,包含 `ok`、`http_status`、`body`(已解析 JSON)、`error`,
  可以直接 `if (result) {...}` 判断是否成功,不使用异常控制流。
- 点的 id 支持数字或字符串(UUID),用 `Point::WithNumericId` / `Point::WithStringId` 构造。
- `filter` 参数直接传 Qdrant 原生的 filter JSON 结构(`must` / `should` / `must_not` 等),
  未做额外封装,以保持对 Qdrant filter DSL 的完整支持。
- 未处理连接池/长连接复用,每次请求创建一个 `CURL*` easy handle;如需高并发场景,
  可以自行改造成基于 `curl_multi` 或维护一个 easy handle 池。
- 未内置重试逻辑,调用方可根据 `ApiResult.http_status` 自行实现重试策略。
