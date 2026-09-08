# 通用后处理器链与业务插件化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立同步通用后处理器链，将 ByteTrack 封装为按业务场景配置启用的 `libtracker.so`，并让唯一的业务动态库只消费带标签名和 `track_id` 的检测结果。

**Architecture:** `InfProcess` 输出原始检测结果，`BusProcess` 通过场景 YAML 创建 `PostProcessorChain`，同步执行启用的后处理器后再调用唯一的 `BusinessPlugin`。模型后处理把 YAML 中解析出的标签名写入 `detection::Object::label_name`；跟踪器按配置的标签名分组维护 ByteTrack 状态并回填 `track_id`。

**Tech Stack:** C++17、CMake、yaml-cpp、POSIX `dlopen`/`dlsym`、ByteTrack、现有 AX SDK/OpenCV 交叉编译环境。

**Spec:** `docs/superpowers/specs/2026-09-08-postprocessor-chain-business-plugin-design.md`

## Global Constraints

- 每路视频只加载一个业务场景动态库，但可按 YAML 顺序配置多个后处理器。
- 后处理器与业务插件在 `BusProcess` 工作线程中同步串行执行；本次不增加独立业务线程。
- 后处理器只加工逐帧数据，不执行告警、上传等业务副作用。
- 业务配置使用解析后的标签名，不使用整数标签 ID；数值 `label` 仅保留用于现有底层算法兼容。
- 跟踪关闭或标签未配置时，对应 box 的 `track_id` 保持 `-1`。
- 已启用动态库缺失、ABI 不匹配或初始化失败时，该路视频启动失败并记录错误。
- 单帧后处理失败时跳过后续处理器和业务插件，但仍把图像发送给 `EncProcess`。
- ByteTrack 源码最终只编译进 `libtracker.so`。
- 本次不实现序列帧累计、状态机或最终汇总判断。
- 不引入新的第三方依赖；宿主与动态库继续使用同一交叉工具链和 C++ 运行时。
- 不在代码注释或日志中输出密钥、Token 或带认证信息的完整流地址。
- 当前环境产物为 AArch64，自动化步骤在当前主机验证“可交叉编译”；测试二进制的行为验证在 ARM 板端执行。

---

## File Structure

### 新增文件

- `models/label_names.h`、`models/label_names.cpp`：统一验证标签索引并向检测对象写入标签名。
- `plugin/component_config.h`：动态组件的公共配置结构。
- `plugin/scene_config.h`、`plugin/scene_config.cpp`：解析主配置中的场景路径以及业务场景 YAML。
- `plugin/postprocessor.h`：后处理器 ABI、上下文和 C 工厂符号。
- `plugin/postprocessor_chain.h`、`plugin/postprocessor_chain.cpp`：动态加载并顺序执行后处理器。
- `plugin/scene_runtime.h`、`plugin/scene_runtime.cpp`：编排后处理器链和唯一业务插件，提供可独立测试的业务运行单元。
- `postprocessors/tracker/tracker_processor.h`、`postprocessors/tracker/tracker_processor.cpp`：ByteTrack 后处理器实现。
- `configs/scenes/face_recognition.yaml`：人脸场景、业务参数和跟踪参数。
- `tests/CMakeLists.txt`：无额外测试框架的 CTest 目标。
- `tests/model_label_names_test.cpp`：标签名传播测试。
- `tests/scene_config_test.cpp`：场景配置解析测试。
- `tests/postprocessor_chain_test.cpp`：动态后处理器加载、顺序和失败测试。
- `tests/tracker_processor_test.cpp`：按标签名跟踪与 ID 回填测试。
- `tests/business_plugin_loader_test.cpp`：单业务插件加载和 ABI 检查测试。
- `tests/scene_runtime_test.cpp`：后处理器链到业务插件的调用边界测试。
- `tests/fakes/*.cpp`：测试用动态后处理器和业务插件。

### 修改文件

- `common/detection_types.h`：增加 `label_name`。
- `models/yolov5.cpp`、`models/scrfd.cpp`：为后处理结果写入标签名。
- `plugin/business_plugin.h`：补充业务插件 ABI 版本和新的配置字段。
- `plugin/plugin_loader.h`、`plugin/plugin_loader.cpp`：从多插件/静态回退改为单配置动态业务插件。
- `plugins/face_recognition/face_plugin.h`、`plugins/face_recognition/face_plugin.cpp`：移除内置 ByteTrack，读取场景参数并消费外部 `track_id`。
- `stages/bus_process.h`：装配场景配置、后处理器链和业务插件，处理逐帧失败与停机顺序。
- `apps/main_rtsp.cpp`：从主配置解析 `pipeline.scene_config` 并传给 `BusProcess`。
- `config.yaml`：声明业务场景配置路径。
- `tracker/bytetrack/BYTETracker.h`、`tracker/bytetrack/BYTETracker.cpp`、`tracker/bytetrack/STrack.h`、`tracker/bytetrack/STrack.cpp`：将 ID 分配状态移入跟踪处理器实例并支持配置阈值。
- `CMakeLists.txt`：构建后处理器、配置文件和测试目标，移除 ByteTrack 重复编译。

### 删除文件

- `plugin/plugin_registry.h`、`plugin/plugin_registry.cpp`：移除业务插件静态回退，保证业务场景按 `.so + YAML` 独立交付。

---

### Task 1: 传播模型配置中的标签名

**Files:**
- Create: `models/label_names.h`
- Create: `models/label_names.cpp`
- Modify: `common/detection_types.h`
- Modify: `models/yolov5.cpp`
- Modify: `models/scrfd.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/model_label_names_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `bool models::AssignLabelNames(const std::vector<std::string>& labels, std::vector<detection::Object>& objects, std::string& error)`
- Produces: `detection::Object::label_name`，其值来自模型 YAML 的 `labels[object.label]`。

- [ ] **Step 1: 建立最小测试入口并写失败测试**

在根 `CMakeLists.txt` 中增加：

```cmake
include(CTest)
option(AX_BUILD_TESTS "Build ax_core tests" OFF)
if(AX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
```

`tests/CMakeLists.txt` 使用以下辅助函数，确保板端可以统一使用 CTest：

```cmake
function(add_ax_test target)
  add_executable(${target} ${ARGN})
  set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
  add_test(NAME ${target} COMMAND ${target})
endfunction()
```

在 `tests/model_label_names_test.cpp` 中构造标签 `{"face", "person"}` 以及两个 `label` 分别为 `1`、`0` 的对象，断言调用后名称分别为 `person`、`face`；再构造 `label == 2`，断言函数返回 `false`，且错误文本包含 `out of range`。

- [ ] **Step 2: 运行构建并确认测试先失败**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAX_BUILD_TESTS=ON
cmake --build build --target model_label_names_test -j2
```

Expected: 因 `AssignLabelNames` 或 `label_name` 尚未定义而编译失败。

- [ ] **Step 3: 添加标签字段和纯函数实现**

在 `detection::Object` 中增加：

```cpp
std::string label_name;
```

实现函数时先完整验证所有 `object.label` 均处于 `[0, labels.size())`，验证通过后再统一赋值，避免失败时只更新了一部分对象：

```cpp
bool AssignLabelNames(const std::vector<std::string>& labels,
                      std::vector<detection::Object>& objects,
                      std::string& error);
```

`Yolov5::Postprocess` 在 `get_out_bbox` 后调用该函数；失败记录模型类型和错误原因并返回非零。`Scrfd::Postprocess` 在最终 `objects` 生成后执行相同调用，不在解码循环中硬编码 `"face"`。

- [ ] **Step 4: 构建测试和生产目标**

Run:

```bash
cmake --build build --target model_label_names_test ax_core -j2
```

Expected: 两个目标均构建成功。

- [ ] **Step 5: 板端运行单元测试**

Run on ARM target:

```bash
./build/tests/model_label_names_test
```

Expected: 退出码 `0`，输出 `model_label_names_test: PASS`。

- [ ] **Step 6: 提交**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/model_label_names_test.cpp common/detection_types.h models/label_names.h models/label_names.cpp models/yolov5.cpp models/scrfd.cpp
git commit -m "feat(models): propagate parsed label names"
```

### Task 2: 解析主配置与单业务场景配置

**Files:**
- Create: `plugin/component_config.h`
- Create: `plugin/scene_config.h`
- Create: `plugin/scene_config.cpp`
- Create: `tests/scene_config_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `config.yaml`
- Create: `configs/scenes/face_recognition.yaml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `plugin::ComponentConfig`
- Produces: `plugin::SceneConfig`
- Produces: `plugin::LoadSceneConfigPath(const std::string&, std::string&, std::string&)`
- Produces: `plugin::LoadSceneConfig(const std::string&, SceneConfig&, std::string&)`
- Produces: `plugin::DefaultAppConfigPath()`，返回可执行文件同目录的 `config.yaml`。

- [ ] **Step 1: 写配置解析失败测试**

定义预期结构：

```cpp
struct ComponentConfig {
  std::string name;
  std::string library_path;
  std::string config_dir;
  std::string params_yaml;
  uint32_t api_version = 0;
  bool enabled = true;
};

struct SceneConfig {
  ComponentConfig scene;
  std::vector<ComponentConfig> postprocessors;
};
```

`tests/scene_config_test.cpp` 使用临时目录创建主配置和场景配置，覆盖以下断言：

- `pipeline.scene_config` 相对主配置目录解析；
- 场景和处理器 `.so` 相对场景配置目录解析；
- `params` 被序列化并保留 `track_labels: [face]`；
- `enabled: false` 的处理器可缺少动态库文件但保留禁用状态；
- 场景缺少 `library`、启用处理器缺少 `api_version` 时返回 `false` 和可读错误；处理器专属参数保持不透明，由对应动态库在 `Init()` 中校验。

- [ ] **Step 2: 构建并确认失败**

Run:

```bash
cmake --build build --target scene_config_test -j2
```

Expected: 因 `SceneConfig` 和加载函数未定义而编译失败。

- [ ] **Step 3: 实现配置解析和路径规则**

使用 yaml-cpp 读取配置，不引入新依赖。`DefaultAppConfigPath()` 通过 `/proc/self/exe` 返回可执行文件同目录的 `config.yaml`。`LoadSceneConfigPath` 只读取主配置的 `pipeline.scene_config`，不改变当前 RTSP/RTMP 参数来源。`LoadSceneConfig` 必须：

- 校验根节点、`version == 1`、必填字段和参数类型；
- 使用 `std::filesystem::absolute(base / relative).lexically_normal()` 解析相对路径；
- 通过 `YAML::Emitter` 序列化每个组件自己的 `params`；
- 对未知字段记录警告；
- 错误字符串包含 YAML 路径和字段名。

- [ ] **Step 4: 添加实际配置和构建复制规则**

在 `config.yaml` 的 `pipeline` 下增加：

```yaml
scene_config: "./configs/face_recognition.yaml"
```

`configs/scenes/face_recognition.yaml` 使用：

```yaml
version: 1
scene:
  name: face_recognition
  library: "../plugins/libface_recognition.so"
  api_version: 1
  params:
    frontal_score_thresh: 0.55
    qdrant_collection: face_embeddings
postprocessors:
  - name: tracker
    enabled: true
    library: "../postprocessors/libtracker.so"
    api_version: 1
    params:
      algorithm: bytetrack
      track_labels: ["face"]
      frame_rate: 25
      track_buffer: 30
      track_thresh: 0.5
      high_thresh: 0.6
      match_thresh: 0.8
```

CMake 将根配置复制到 `${CMAKE_BINARY_DIR}/config.yaml`，将场景配置复制到 `${CMAKE_BINARY_DIR}/configs/face_recognition.yaml`。

- [ ] **Step 5: 构建并在板端运行测试**

Run:

```bash
cmake --build build --target scene_config_test -j2
```

Run on ARM target:

```bash
./build/tests/scene_config_test
```

Expected: 退出码 `0`，输出 `scene_config_test: PASS`。

- [ ] **Step 6: 提交**

```bash
git add CMakeLists.txt config.yaml configs/scenes/face_recognition.yaml plugin/component_config.h plugin/scene_config.h plugin/scene_config.cpp tests/CMakeLists.txt tests/scene_config_test.cpp
git commit -m "feat(config): add scene runtime configuration"
```

### Task 3: 建立后处理器 ABI 与动态执行器链

**Files:**
- Create: `plugin/postprocessor.h`
- Create: `plugin/postprocessor_chain.h`
- Create: `plugin/postprocessor_chain.cpp`
- Create: `tests/postprocessor_chain_test.cpp`
- Create: `tests/fakes/postprocessor_append.cpp`
- Create: `tests/fakes/postprocessor_fail.cpp`
- Create: `tests/fakes/postprocessor_bad_version.cpp`
- Create: `tests/fakes/no_factory.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `plugin::ComponentConfig`
- Produces: `plugin::PostProcessContext`
- Produces: `PostProcessor`、`CreatePostProcessorFn`、`DestroyPostProcessorFn`
- Produces: `plugin::PostProcessorChain::Load`、`Process`、`Unload`

- [ ] **Step 1: 写动态加载和顺序失败测试**

定义 ABI：

```cpp
inline constexpr uint32_t kPostProcessorApiVersion = 1;

struct PostProcessContext {
  const ImageData& frame;
  std::vector<detection::Object>& objects;
  uint64_t frame_seq;
  std::chrono::steady_clock::time_point timestamp;
};

class PostProcessor {
public:
  virtual ~PostProcessor() = default;
  virtual const char* Name() const = 0;
  virtual uint32_t ApiVersion() const = 0;
  virtual int Init(const plugin::ComponentConfig& config) = 0;
  virtual int Process(PostProcessContext& context) = 0;
  virtual void Shutdown() = 0;
};
```

测试动态库行为：`postprocessor_append` 从 `params_yaml` 读取 `digit`，先将负数 `track_id` 视为 `0`，再执行 `track_id = base * 10 + digit`；连续配置 `digit: 1`、`digit: 2` 后得到 `12`，用于证明执行顺序。`postprocessor_fail` 的 `Process()` 返回 `-77`，并在参数 `fail_init: true` 时让 `Init()` 返回 `-66`；`postprocessor_bad_version` 返回版本 `999`；`no_factory` 动态库不导出工厂符号。

为验证逆序清理，测试处理器从参数读取 `lifecycle_file`，在 `Shutdown()` 中向该临时文件追加自己的名称；测试断言文件内容与加载顺序相反。

测试断言：

- 两个 append 处理器按配置顺序执行并得到 `track_id == 12`；
- 中间处理器返回 `-77` 后，后续处理器不执行；
- 缺失 `.so`、缺失工厂符号、版本 `999`、`Init()` 失败均导致 `Load()` 失败；
- 部分加载失败时已加载组件按逆序执行 `Shutdown()` 和销毁。

- [ ] **Step 2: 构建并确认失败**

Run:

```bash
cmake --build build --target postprocessor_chain_test -j2
```

Expected: 因后处理器 ABI 和 `PostProcessorChain` 尚未定义而编译失败。

- [ ] **Step 3: 实现后处理器接口和加载器**

`PostProcessorChain` 的公开接口固定为：

```cpp
class PostProcessorChain {
public:
  ~PostProcessorChain();
  int Load(const std::vector<ComponentConfig>& configs);
  int Process(PostProcessContext& context);
  void Unload();
  size_t Size() const;
};
```

加载流程依次执行 `dlopen(RTLD_NOW | RTLD_LOCAL)`、解析 `CreatePostProcessor`/`DestroyPostProcessor`、创建实例、检查配置版本和实例版本、调用 `Init()`。禁用组件直接跳过。任一步失败都记录组件名、路径和阶段，并调用 `Unload()` 回滚。

卸载按逆序执行 `Shutdown()`、`DestroyPostProcessor()`、`dlclose()`，且重复调用安全。

- [ ] **Step 4: 构建并在板端运行测试**

Run:

```bash
cmake --build build --target postprocessor_chain_test -j2
```

Run on ARM target:

```bash
./build/tests/postprocessor_chain_test
```

Expected: 退出码 `0`，输出 `postprocessor_chain_test: PASS`。

- [ ] **Step 5: 提交**

```bash
git add CMakeLists.txt plugin/postprocessor.h plugin/postprocessor_chain.h plugin/postprocessor_chain.cpp tests/CMakeLists.txt tests/postprocessor_chain_test.cpp tests/fakes/postprocessor_append.cpp tests/fakes/postprocessor_fail.cpp tests/fakes/postprocessor_bad_version.cpp tests/fakes/no_factory.cpp
git commit -m "feat(postprocess): add dynamic processor chain"
```

### Task 4: 将 ByteTrack 封装为 libtracker.so

**Files:**
- Create: `postprocessors/tracker/tracker_processor.h`
- Create: `postprocessors/tracker/tracker_processor.cpp`
- Modify: `tracker/bytetrack/BYTETracker.h`
- Modify: `tracker/bytetrack/BYTETracker.cpp`
- Modify: `tracker/bytetrack/STrack.h`
- Modify: `tracker/bytetrack/STrack.cpp`
- Create: `tests/tracker_processor_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `PostProcessor`、`PostProcessContext`、`ComponentConfig::params_yaml`、`Object::label_name`
- Produces: `libtracker.so`
- Produces: `BYTETracker::update(const std::vector<detection::Object>&, int& next_track_id)`

- [ ] **Step 1: 写按名称过滤和稳定 ID 的失败测试**

通过 `PostProcessorChain` 动态加载 `libtracker.so`，参数使用：

```yaml
algorithm: bytetrack
track_labels: [face]
frame_rate: 25
track_buffer: 30
track_thresh: 0.5
high_thresh: 0.6
match_thresh: 0.8
```

首帧传入高置信度 `face` 和 `smoke`，断言 `face.track_id > 0`、`smoke.track_id == -1`；下一帧传入轻微位移的同一 `face`，断言 ID 不变。再以 `[face, person]` 初始化新实例，断言同帧两类目标均有 ID 且 ID 不相等。空名称、未知算法、阈值越界必须导致 `Load()` 失败。

- [ ] **Step 2: 构建并确认失败**

Run:

```bash
cmake --build build --target tracker_processor_test -j2
```

Expected: 因 `libtracker.so` 目标不存在而失败。

- [ ] **Step 3: 使 ByteTrack 参数和 ID 分配实例化**

新增配置结构：

```cpp
struct BYTETrackerConfig {
  int frame_rate = 30;
  int track_buffer = 30;
  float track_thresh = 0.5f;
  float high_thresh = 0.6f;
  float match_thresh = 0.8f;
};
```

`BYTETracker` 构造函数接收该结构。删除 `STrack::next_id()` 内的函数静态计数器，将激活接口改为 `STrack::activate(byte_kalman::ByteKalmanFilter&, int frame_id, int track_id)`。`BYTETracker::update` 接收由上层持有的 `int& next_track_id`，新轨激活时使用并递增，确保同一 `TrackerProcessor` 的多个标签共享一个 ID 序列。

- [ ] **Step 4: 实现 TrackerProcessor**

`TrackerProcessor` 保存：

```cpp
std::unordered_set<std::string> track_labels_;
std::unordered_map<std::string, std::unique_ptr<BYTETracker>> trackers_;
int next_track_id_ = 1;
```

`Init()` 解析并严格校验参数。`Process()` 先把所有对象 `track_id` 设为 `-1`，再按 `label_name` 筛选和分组；每个名称调用独立 `BYTETracker`。轨迹输出只与同名原始检测框做 IoU 匹配，匹配阈值沿用现有 `0.1f`，将 ID 回填到原 vector 中。

导出：

```cpp
extern "C" PostProcessor* CreatePostProcessor();
extern "C" void DestroyPostProcessor(PostProcessor* processor);
```

- [ ] **Step 5: 配置 CMake 动态库产物**

新增 `tracker_postprocessor` SHARED 目标，`OUTPUT_NAME tracker`，输出目录 `${CMAKE_BINARY_DIR}/postprocessors`，RPATH 为 `$ORIGIN/..`。此任务暂时保留现有主程序和人脸插件中的 ByteTrack 源码，避免中间提交无法构建；重复编译在 Task 6 和 Task 7 中依次移除。

- [ ] **Step 6: 构建并在板端运行测试**

Run:

```bash
cmake --build build --target tracker_postprocessor tracker_processor_test -j2
```

Run on ARM target:

```bash
./build/tests/tracker_processor_test
```

Expected: 退出码 `0`，输出 `tracker_processor_test: PASS`。

- [ ] **Step 7: 提交**

```bash
git add CMakeLists.txt postprocessors/tracker/tracker_processor.h postprocessors/tracker/tracker_processor.cpp tracker/bytetrack/BYTETracker.h tracker/bytetrack/BYTETracker.cpp tracker/bytetrack/STrack.h tracker/bytetrack/STrack.cpp tests/CMakeLists.txt tests/tracker_processor_test.cpp
git commit -m "feat(tracker): add ByteTrack postprocessor library"
```

### Task 5: 将业务加载器收敛为单场景动态库

**Files:**
- Modify: `plugin/business_plugin.h`
- Modify: `plugin/plugin_loader.h`
- Modify: `plugin/plugin_loader.cpp`
- Delete: `plugin/plugin_registry.h`
- Delete: `plugin/plugin_registry.cpp`
- Modify: `plugins/face_recognition/face_plugin.h`
- Modify: `plugins/face_recognition/face_plugin.cpp`
- Create: `tests/business_plugin_loader_test.cpp`
- Create: `tests/fakes/business_plugin_ok.cpp`
- Create: `tests/fakes/business_plugin_bad_version.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `plugin::ComponentConfig`
- Produces: `BusinessPlugin::ApiVersion()`
- Produces: `plugin::PluginManager::Load(HostServices*, const ComponentConfig&)`

- [ ] **Step 1: 写单插件加载失败测试**

测试插件 `business_plugin_ok` 的 `OnFrame()` 在首个对象 `track_id == 42` 时返回 `0`，否则返回 `-42`；当 `params_yaml` 包含 `fail_init: true` 时，`Init()` 返回 `-66`。复用 `no_factory` 动态库覆盖工厂缺失，并使用 `business_plugin_bad_version` 覆盖 API 版本错误。测试覆盖成功加载并调用、库缺失、工厂缺失、API 版本错误、初始化失败、重复 `Unload()`。

- [ ] **Step 2: 构建并确认失败**

Run:

```bash
cmake --build build --target business_plugin_loader_test -j2
```

Expected: 因新 `Load` 签名和 `ApiVersion()` 尚未实现而失败。

- [ ] **Step 3: 更新业务 ABI 与配置对象**

`BusinessPlugin` 增加：

```cpp
inline constexpr uint32_t kBusinessPluginApiVersion = 1;
virtual uint32_t ApiVersion() const = 0;
```

`PluginConfig` 改为包含 `name`、`library_path`、`config_dir`、`params_yaml` 和 `api_version`。`FacePlugin::ApiVersion()` 返回 `1`，暂不改动其跟踪逻辑。

- [ ] **Step 4: 重写 PluginManager 为单动态插件**

公开接口固定为：

```cpp
int Load(HostServices* host, const ComponentConfig& config);
int OnFrame(const ImageData& frame,
            const std::vector<detection::Object>& objects);
void Unload();
bool Loaded() const;
```

删除逗号列表、环境变量目录和静态注册表回退。加载失败记录场景名、库路径和失败阶段；卸载继续执行 `WaitQuiesce -> Shutdown -> DestroyPlugin -> dlclose`。

- [ ] **Step 5: 移除主程序中的静态业务插件**

从 `ax_core` 源文件中删除 `plugins/face_recognition/face_plugin.cpp`、`plugin/plugin_registry.cpp`，删除注册表文件。`libface_recognition.so` 仍作为独立目标构建。

- [ ] **Step 6: 构建并在板端运行测试**

Run:

```bash
cmake --build build --target business_plugin_loader_test ax_core face_recognition -j2
```

Run on ARM target:

```bash
./build/tests/business_plugin_loader_test
```

Expected: 退出码 `0`，输出 `business_plugin_loader_test: PASS`。

- [ ] **Step 7: 提交**

```bash
git add -A plugin/business_plugin.h plugin/plugin_loader.h plugin/plugin_loader.cpp plugin/plugin_registry.h plugin/plugin_registry.cpp plugins/face_recognition/face_plugin.h plugins/face_recognition/face_plugin.cpp tests CMakeLists.txt
git commit -m "refactor(plugin): load one configured business scene"
```

### Task 6: 迁移 FacePlugin 使用外部 track_id

**Files:**
- Modify: `plugins/face_recognition/face_plugin.h`
- Modify: `plugins/face_recognition/face_plugin.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `PluginConfig::params_yaml`
- Consumes: `detection::Object::track_id`
- Preserves: `BusinessPlugin::OnFrame(const ImageData&, const std::vector<detection::Object>&)`

- [ ] **Step 1: 记录迁移前静态检查结果**

Run:

```bash
rg -n "BYTETracker|face_tracker_|rect_iou|auto tracks" plugins/face_recognition
```

Expected: 命中 `face_plugin.h` 和 `face_plugin.cpp`，证明业务插件仍内置跟踪。

- [ ] **Step 2: 从 FacePlugin 移除跟踪算法**

删除 `BYTETracker.h`、`face_tracker_`、`rect_iou`、`face_used`、`tracks` 以及轨迹到检测框的二次匹配代码。`OnFrame()` 直接使用传入对象：

```cpp
const auto& tracked_faces = objects;
```

每帧递增现有 `frame_seq_`，只处理 `track_id >= 0` 的对象。用当前帧出现的 `track_id` 更新 `track_pending_[id].last_seen_frame`；保留正脸门控、特征提取、异步检索、丢轨超时和告警逻辑。

- [ ] **Step 3: 从场景参数初始化 FacePlugin**

使用 yaml-cpp 解析 `cfg.params_yaml`，读取：

```yaml
frontal_score_thresh: 0.55
qdrant_collection: face_embeddings
```

缺失字段使用当前默认值；类型错误、阈值不在 `[0, 1]`、空 collection 时 `Init()` 返回非零并记录字段名。停止从 `HostServices::ConfigFloat/ConfigString` 获取这两个业务参数。

- [ ] **Step 4: 移除人脸动态库对 ByteTrack 源码的编译**

从 `face_recognition` 目标删除 `${TRACKER_SRCS}`，保留 `ax_runtime` 链接。

- [ ] **Step 5: 构建并运行静态验证**

Run:

```bash
cmake --build build --target face_recognition -j2
rg -n "BYTETracker|face_tracker_|rect_iou|auto tracks" plugins/face_recognition
```

Expected: 动态库构建成功，`rg` 无输出且退出码为 `1`。

- [ ] **Step 6: 提交**

```bash
git add CMakeLists.txt plugins/face_recognition/face_plugin.h plugins/face_recognition/face_plugin.cpp
git commit -m "refactor(face): consume external tracking results"
```

### Task 7: 在 BusProcess 集成场景配置和后处理器链

**Files:**
- Modify: `stages/bus_process.h`
- Modify: `apps/main_rtsp.cpp`
- Create: `plugin/scene_runtime.h`
- Create: `plugin/scene_runtime.cpp`
- Create: `tests/scene_runtime_test.cpp`
- Create: `tests/fakes/postprocessor_set_track.cpp`
- Modify: `tests/fakes/business_plugin_ok.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `LoadSceneConfigPath`、`LoadSceneConfig`、`PostProcessorChain`、单场景 `PluginManager`
- Produces: `plugin::SceneRuntime::Init`、`Process`、`Shutdown`
- Produces: `BusProcess::BusProcess(std::string scene_config_path)`
- Preserves: 后处理失败时仍发送 `kMsgBusprocData` 到 `EncProcess`

- [ ] **Step 1: 写链到业务插件的集成失败测试**

`postprocessor_set_track` 把首个对象的 `track_id` 设置为 `42`；`business_plugin_ok` 只在收到 `42` 时返回成功。测试通过 `SceneRuntime` 装配真实 `PostProcessorChain` 和 `PluginManager`，断言：

- 后处理成功后业务插件返回 `0`；
- 处理器返回 `-77` 时不调用业务插件；
- 初始化中途失败后，业务插件和处理器都能重复卸载。

- [ ] **Step 2: 构建并确认失败**

Run:

```bash
cmake --build build --target scene_runtime_test -j2
```

Expected: 因 `SceneRuntime` 尚未定义而编译失败。

- [ ] **Step 3: 实现可独立测试的 SceneRuntime**

定义明确的运行结果：

```cpp
enum class SceneProcessStage {
  kOk,
  kPostProcessor,
  kBusinessPlugin,
};

struct SceneProcessResult {
  SceneProcessStage stage = SceneProcessStage::kOk;
  int code = 0;
  bool Ok() const { return code == 0; }
};
```

`SceneRuntime` 接口固定为：

```cpp
int Init(HostServices* host, const SceneConfig& config);
SceneProcessResult Process(const ImageData& frame,
                           std::vector<detection::Object>& objects,
                           uint64_t frame_seq);
void Shutdown();
```

`Process()` 先把所有 `track_id` 重置为 `-1`，构造 `PostProcessContext` 并执行后处理器链；失败时返回 `kPostProcessor` 且不调用业务插件。链成功后调用业务插件，并以 `kBusinessPlugin` 返回其错误。`Shutdown()` 先卸载业务插件，再卸载后处理器，重复调用安全。

- [ ] **Step 4: 构建并在板端运行 SceneRuntime 测试**

Run:

```bash
cmake --build build --target scene_runtime_test -j2
```

Run on ARM target:

```bash
./build/tests/scene_runtime_test
```

Expected: 退出码 `0`，输出 `scene_runtime_test: PASS`。

- [ ] **Step 5: 让 main 解析场景配置路径**

`main_rtsp.cpp` 使用 `argv[1]` 作为可选主配置路径；未提供时调用 `plugin::DefaultAppConfigPath()`，默认读取与 `ax_core` 同目录的 `config.yaml`。调用：

```cpp
std::string scene_config_path;
std::string config_error;
if (!plugin::LoadSceneConfigPath(app_config_path, scene_config_path,
                                 config_error)) {
  LOG_ERROR("load scene config path failed: {}", config_error);
  return -1;
}
```

将路径传给 `new BusProcess(scene_config_path)`。日志不得输出 RTSP/RTMP 中的认证信息。

- [ ] **Step 6: 重写 BusProcess 初始化和每帧调用顺序**

`BusProcess::Init()` 顺序：查找 `EncProcess`、初始化 `HostServices`、解析 `SceneConfig`、调用 `SceneRuntime::Init()`。任一步失败都先关闭 `SceneRuntime`，再关闭 `HostServices` 并返回非零。

每帧处理顺序：

```cpp
const auto result = runtime_.Process(
    in_data->image, in_data->objects, ++frame_seq_);
pipeline::SendMessage(next_thread_id_, kMsgBusprocData, out_data);
```

帧映射和 `Unmap()` 沿用现有保护。无论 `result` 是否成功，都执行向 `EncProcess` 的发送。

- [ ] **Step 7: 增加热路径错误限频**

在 `BusProcess` 内根据 `SceneProcessResult::stage` 分别维护后处理器和业务插件失败 streak。行为与 `InfProcess::ReportFailure/ReportRecovered` 一致：首错立即记录，每 5 秒汇总一次持续失败，恢复时记录持续秒数和跳过帧数。日志包含失败阶段和返回码，不每帧刷屏。

- [ ] **Step 8: 修正停机顺序和构建边界**

停止投喂后，先调用 `runtime_.Shutdown()`，由它等待并卸载业务插件后再逆序卸载后处理器，最后调用 `host_.Shutdown()`。析构和 `kMsgAppExit` 共用幂等 `Shutdown()` 私有函数。

从 `ax_core` 目标删除 `${TRACKER_SRCS}`，使 ByteTrack 最终只存在于 `tracker_postprocessor` 目标。

- [ ] **Step 9: 构建全部生产目标**

Run:

```bash
cmake --build build --target scene_runtime_test ax_core tracker_postprocessor face_recognition -j2
```

Expected: `scene_runtime_test`、`ax_core`、`libtracker.so` 和 `libface_recognition.so` 均构建成功。

- [ ] **Step 10: 提交**

```bash
git add CMakeLists.txt apps/main_rtsp.cpp stages/bus_process.h plugin/scene_runtime.h plugin/scene_runtime.cpp tests/CMakeLists.txt tests/scene_runtime_test.cpp tests/fakes/postprocessor_set_track.cpp tests/fakes/business_plugin_ok.cpp
git commit -m "feat(pipeline): integrate configured postprocessing chain"
```

### Task 8: 完成构建产物检查和板端验收清单

**Files:**
- Modify: `build.sh`
- Modify: `run.sh`

**Interfaces:**
- Consumes: 最终 `ax_core`、`libax_runtime.so`、`plugins/libface_recognition.so`、`postprocessors/libtracker.so` 和复制后的 YAML。
- Produces: 可重复的交叉构建命令和板端手动验证入口。

- [ ] **Step 1: 让构建脚本显式构建全部交付物**

`build.sh` 保持现有交叉编译方式，增加 `-DAX_BUILD_TESTS=ON`，并构建默认 `all` 目标，使生产动态库和测试二进制都被编译：

```bash
cmake --build build -j2
```

不得在宿主机安装新依赖。

- [ ] **Step 2: 更新运行脚本的配置入口**

`run.sh` 调用：

```bash
./build/ax_core ./build/config.yaml
```

保留现有板端动态库环境设置；不得在输出中打印包含用户名或密码的完整流地址。

- [ ] **Step 3: 执行干净交叉构建**

Run:

```bash
./build.sh
```

Expected: `ax_core`、`libax_runtime.so`、`plugins/libface_recognition.so`、`postprocessors/libtracker.so` 全部构建成功。

- [ ] **Step 4: 验证 ByteTrack 没有重复编译**

Run:

```bash
nm -C build/ax_core | rg "BYTETracker|STrack::"
nm -C build/plugins/libface_recognition.so | rg "BYTETracker|STrack::"
nm -C build/postprocessors/libtracker.so | rg "BYTETracker|STrack::"
```

Expected: 前两个命令无输出；第三个命令能找到 ByteTrack/`STrack` 符号。

- [ ] **Step 5: 验证配置和动态依赖产物**

Run:

```bash
test -f build/config.yaml
test -f build/configs/face_recognition.yaml
test -f build/plugins/libface_recognition.so
test -f build/postprocessors/libtracker.so
readelf -d build/plugins/libface_recognition.so | rg "RPATH|RUNPATH"
readelf -d build/postprocessors/libtracker.so | rg "RPATH|RUNPATH"
```

Expected: 文件全部存在，两个动态库的 RPATH/RUNPATH 均可解析上一级目录中的 `libax_runtime.so`。

- [ ] **Step 6: 运行全部板端测试**

Run on ARM target:

```bash
ctest --test-dir build --output-on-failure
```

Expected: `model_label_names_test`、`scene_config_test`、`postprocessor_chain_test`、`tracker_processor_test`、`business_plugin_loader_test`、`scene_runtime_test` 全部通过。

- [ ] **Step 7: 执行用户负责的板端业务冒烟测试**

按以下固定清单手动验证并记录结果：

1. `track_labels: ["face"]` 时，同一人脸连续帧的 `track_id` 稳定。
2. 跟踪处理器 `enabled: false` 时，业务插件继续运行且所有 `track_id == -1`。
3. 配置未包含的标签保持 `track_id == -1`。
4. 分别制造动态库缺失、API 版本错误和 YAML 参数错误，程序启动失败且日志包含组件名、库路径和失败阶段。
5. 后处理器单帧失败时，本帧不进入业务插件，但编码输出不中断；恢复后继续执行业务插件。
6. 人脸识别、陌生人告警和异步推送保持原行为。
7. 连续运行后正常退出，无崩溃、死锁或明显资源泄漏。

- [ ] **Step 8: 提交构建脚本调整**

```bash
git add build.sh run.sh
git commit -m "chore(build): package scene plugins and tracker"
```

---

## Final Verification

- [ ] `git diff --check` 无输出。
- [ ] `git status --short` 仅包含预期改动，提交后为空。
- [ ] `./build.sh` 成功。
- [ ] `nm` 证明 ByteTrack 仅位于 `libtracker.so`。
- [ ] 配置文件和两个插件动态库位于预期构建目录。
- [ ] 板端 CTest 由用户执行并反馈结果。
- [ ] 板端业务冒烟测试由用户执行并反馈结果。
- [ ] 确认没有实现序列帧累计逻辑。
