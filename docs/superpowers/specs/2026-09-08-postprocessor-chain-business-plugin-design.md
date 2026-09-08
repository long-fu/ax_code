# 通用后处理器链与业务场景插件化设计

## 1. 背景

当前工程已经具备 `BusinessPlugin`、`PluginManager` 和 `dlopen` 驱动的业务插件机制，`face_recognition` 也可以构建为独立动态库。但目标跟踪仍直接编译进主程序和人脸业务插件，ByteTrack 的调用、检测框关联和 `track_id` 回填均位于 `FacePlugin::OnFrame` 内。

后续每路视频流会接入一个独立业务场景。每个场景以一个 `.so` 动态库和一个对应的 YAML 配置文件交付。业务代码只消费带 `track_id` 的检测框，不应了解具体跟踪算法；目标跟踪是否启用由当前业务场景配置决定。

## 2. 目标

- 每个业务场景独立构建为一个 `.so`，并拥有自己的 YAML 配置文件。
- 建立可按配置加载、按声明顺序同步执行的通用后处理器链。
- 将 ByteTrack 封装为通用 `libtracker.so` 后处理器。
- 跟踪启用时，为配置的检测标签名称回填 `track_id`；关闭时保持 `-1`。
- 后处理器只加工逐帧数据；业务插件负责告警、上传、绘制和跨帧状态。
- 同一路视频只加载一个业务场景，但可配置多个通用后处理器。
- 动态库缺失、ABI 不匹配或初始化失败时，阻止该路视频启动并记录清晰错误。

## 3. 非目标

- 本次不实现序列帧累计、状态机或最终汇总判断框架。
- 不支持同一路视频同时运行多个业务场景。
- 不引入独立业务线程；后处理器和业务插件保持同步串行调用。
- 不把业务插件放入后处理器链。
- 不实现通用算法注册表或算法热切换；首个跟踪实现固定为 ByteTrack。

序列帧能力将在后续单独设计。后处理上下文先保留 `frame_seq` 和时间戳；业务插件接口是否需要这些字段，在序列帧设计中再决定，本次不提前扩展。

## 4. 总体架构

单路视频的数据流为：

```text
PreProcess
    -> InfProcess（推理与检测后处理）
    -> BusProcess
         -> PostProcessorChain
              -> processor 1
              -> libtracker.so（按场景配置可选）
              -> processor N
         -> BusinessPlugin（唯一业务场景）
    -> EncProcess
```

### 4.1 InfProcess

`InfProcess` 只负责模型推理及模型自身的检测后处理，输出原始 `detection::Object`。它不读取业务场景配置，也不感知跟踪是否启用。

### 4.2 BusProcess

`BusProcess` 是业务阶段的编排者，负责：

1. 读取当前业务场景 YAML。
2. 按配置顺序创建 `PostProcessorChain`。
3. 加载唯一的业务场景动态库。
4. 每帧先同步执行后处理器链，再调用业务插件。
5. 即使某帧业务处理失败，也继续把图像帧发送给 `EncProcess`，保持视频编码链路连续。
6. 按确定的顺序完成停机和资源释放。

### 4.3 PostProcessorChain

后处理器链只负责逐帧加工检测结果，例如目标跟踪、类别过滤、区域预处理或属性补充。处理器可以修改检测对象或为上下文补充数据，但不得执行告警、上传等业务副作用。

链按 YAML 中的声明顺序同步执行。配置为 `enabled: false` 的处理器不加载、不初始化，也不进入运行链。

### 4.4 BusinessPlugin

业务插件只消费后处理器链完成后的最终结果，负责当前场景的业务行为。插件允许在实例内部维护跨帧状态，但本次不提供通用序列帧框架。

## 5. 动态库接口

### 5.1 后处理上下文

公共接口定义一个后处理上下文，至少包含：

- 当前帧，只读访问；
- 可变的 `std::vector<detection::Object>`；
- 单调递增的 `frame_seq`；
- 当前帧时间戳。

进入链之前，宿主保证所有检测对象的 `track_id` 默认值为 `-1`。每个后处理器只能在自己的职责范围内修改上下文。

`detection::Object` 增加 `label_name` 字段。模型后处理根据模型 YAML 中已经解析的 `labels` 列表，同时写入整数 `label` 和字符串 `label_name`：整数 ID 继续供现有底层算法兼容使用，后处理器和业务配置使用可读的 `label_name`。

### 5.2 PostProcessor 接口

```cpp
class PostProcessor {
public:
    virtual ~PostProcessor() = default;

    virtual const char* Name() const = 0;
    virtual uint32_t ApiVersion() const = 0;
    virtual int Init(const PostProcessorConfig& config) = 0;
    virtual int Process(PostProcessContext& context) = 0;
    virtual void Shutdown() = 0;
};
```

每个后处理器动态库导出 C ABI 工厂：

```cpp
extern "C" PostProcessor* CreatePostProcessor();
extern "C" void DestroyPostProcessor(PostProcessor* processor);
```

宿主和动态库使用同一交叉工具链与 C++ 运行时。`ApiVersion()` 用于启动时快速发现接口版本不匹配，避免加载后发生未定义行为。

### 5.3 BusinessPlugin 接口

业务插件保留独立接口，接收只读的最终检测结果：

```cpp
virtual int OnFrame(
    const ImageData& frame,
    const std::vector<detection::Object>& objects) = 0;
```

业务插件不得直接依赖 ByteTrack，也不得修改共享检测结果。现有异步上传、向量检索等耗时任务仍通过 `HostServices::SubmitAsync` 提交，并纳入插件停机时的在途任务统计。

### 5.4 配置传递

宿主解析动态库加载所需的通用字段。每个动态库只接收属于自己的配置子树，使用序列化文本传递，避免插件依赖整份场景配置的结构。

配置对象至少包含：

- 组件名称；
- 动态库路径；
- API 版本；
- 当前组件的参数文本；
- 当前业务配置文件所在目录，用于解析相对路径。

## 6. 业务场景配置

每个业务场景由一个 `.so` 和一个 YAML 配置文件组成。配置示例：

```yaml
version: 1

scene:
  name: face_recognition
  library: ./plugins/libface_recognition.so
  api_version: 1
  params:
    frontal_score_thresh: 0.55
    qdrant_collection: face_embeddings

postprocessors:
  - name: tracker
    enabled: true
    library: ./postprocessors/libtracker.so
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

主配置通过 `pipeline.scene_config` 指定当前视频流对应的业务场景配置文件：

```yaml
pipeline:
  scene_config: ./configs/scenes/face_recognition.yaml
```

动态库相对路径以业务场景配置文件所在目录为基准解析，避免依赖进程启动目录。

配置校验规则：

- `scene.name`、`scene.library` 和 `scene.api_version` 必填。
- `postprocessors` 可为空。
- 启用的处理器必须提供 `name`、`library` 和 `api_version`。
- `track_labels` 只接受非空字符串，名称与模型配置中的标签名完全匹配，重复名称在初始化时去重。
- 阈值和缓冲参数必须在各自合法范围内。
- 未知字段记录警告但不阻止启动，以便配置平滑演进。

## 7. libtracker.so 设计

`libtracker.so` 实现 `PostProcessor` 接口，并在内部封装 ByteTrack。

### 7.1 标签选择

只有 `params.track_labels` 中声明的标签名参与跟踪。跟踪处理器直接读取每个检测对象的 `label_name`，业务配置不使用整数标签 ID。其他对象保持 `track_id = -1`。

每个标签名维护独立的 ByteTrack 状态，防止不同类别之间发生关联。跟踪器实例为其输出生成唯一 ID，保证同一视频流、同一业务插件生命周期内不同标签的 `track_id` 不冲突。

### 7.2 检测框回填

ByteTrack 输出轨迹后，跟踪处理器在同标签检测集合中完成轨迹与原始检测框的匹配，将 ID 回填到原对象。未成功关联到激活轨迹的对象保持 `-1`。

匹配逻辑属于 `libtracker.so`，不得留在业务插件中。

### 7.3 状态和线程约束

跟踪器是有状态组件，一个实例只服务一路视频流。`Process()` 在 `BusProcess` 工作线程中串行调用，不要求内部并发。状态在 `Shutdown()` 或实例销毁时完整释放。

本次实现需要消除 ByteTrack ID 分配中的函数静态可变状态，使 ID 分配归属于跟踪处理器实例，避免未来多路流并发时的数据竞争和生命周期串扰。

### 7.4 构建边界

ByteTrack 源码只编译进 `libtracker.so`。主程序和 `libface_recognition.so` 均不得再直接链接或编译 ByteTrack 源文件。

## 8. 每帧数据流

1. `InfProcess` 完成推理和检测后处理，创建同时包含 `label` 与 `label_name` 的原始检测对象。
2. `BusProcess` 接收帧，分配递增的 `frame_seq`，并构造 `PostProcessContext`。
3. 宿主将所有输入对象的 `track_id` 规范化为 `-1`。
4. `PostProcessorChain` 按配置顺序同步执行启用的处理器。
5. 如果跟踪处理器启用，它只处理配置标签并回填 `track_id`。
6. 全部处理器成功后，业务插件接收最终检测结果。
7. 业务插件可以使用 `track_id` 处理本帧业务，但本次不实现序列累计。
8. 无论本帧是否进入业务处理，图像帧都继续发送给 `EncProcess`，保持输出流连续。

跟踪关闭时不加载 `libtracker.so`，第 4 步跳过该组件，业务插件收到的检测对象 `track_id` 全为 `-1`。

## 9. 错误处理

### 9.1 启动错误

以下情况均采用 fail-fast，阻止该视频流启动：

- 业务配置文件不存在或 YAML 语法错误；
- 必填字段缺失或参数越界；
- 已启用动态库不存在；
- `dlopen` 失败；
- 工厂符号缺失；
- 工厂返回空指针；
- API 版本不匹配；
- 组件 `Init()` 失败。

错误日志必须包含组件类型、组件名、库路径、失败阶段和错误原因。启动失败时，已成功初始化的组件按逆序清理。

### 9.2 单帧运行错误

任一后处理器 `Process()` 返回失败时：

1. 立即终止当前帧的后续处理器调用。
2. 不调用当前帧的业务插件，避免其消费部分加工的结果。
3. 仍将图像帧发送给 `EncProcess`，保持视频输出连续。
4. 使用首错立即记录、持续错误按时间窗口汇总、恢复时记录一次的限频策略，避免高帧率下刷屏。

业务插件 `OnFrame()` 失败时同样记录限频错误，但不阻断编码链路。下一帧继续调用业务插件，是否清理其内部状态由插件自己的错误处理负责。

### 9.3 停机顺序

停机顺序固定为：

1. 停止上游继续投喂新帧。
2. 等待业务插件通过宿主提交的异步任务归零。
3. 调用业务插件 `Shutdown()`，销毁实例并关闭动态库。
4. 按逆序调用后处理器 `Shutdown()`，销毁实例并关闭动态库。
5. 释放 `HostServices` 和其他宿主资源。

所有关闭操作必须幂等，覆盖正常退出、启动中途失败和析构兜底路径。

## 10. 现有人脸业务迁移

`FacePlugin` 的迁移范围：

- 删除 `BYTETracker` 成员和相关头文件依赖。
- 删除 `tracked_faces` 的内部跟踪调用及 IoU 回填逻辑。
- 直接使用输入对象上的 `track_id`。
- 保留按轨迹管理特征提取、识别结果、丢轨和告警的现有业务逻辑。
- 保留异步向量检索及推送机制。
- 根据独立场景配置读取人脸业务参数。

迁移后，业务插件无法区分“跟踪关闭”和“当前框未关联到轨迹”，两者都表现为 `track_id = -1`。这是本次明确约定；需要区分时再为上下文增加处理器状态，不提前扩展接口。

## 11. 测试与验收

### 11.1 可自动执行的验证

- 配置解析：正常配置、空处理器链、禁用处理器、字段缺失、空标签名和参数越界。
- 动态加载：成功加载、库缺失、符号缺失、ABI 不匹配、初始化失败及逆序清理。
- 链执行：严格按配置顺序执行；前序失败后停止后续处理器和业务插件。
- 跟踪器：按 `label_name` 过滤、ID 稳定、未跟踪标签为 `-1`、多标签不交叉关联、关闭时全为 `-1`。
- `BusProcess` 集成：成功时业务插件收到回填后的 ID；处理器失败时跳过业务但继续发送编码帧。
- 构建产物：生成 `libtracker.so`；主程序和业务插件不再包含 ByteTrack 对象代码。

### 11.2 板端手动验证

由用户后续在目标设备手动验证：

- 跟踪开启时，同一目标连续帧获得稳定 `track_id`。
- 跟踪关闭时，业务插件正常运行且 `track_id` 为 `-1`。
- 配置标签之外的目标不参与跟踪。
- 动态库缺失、非法配置和 ABI 不匹配时启动失败并输出明确日志。
- 业务插件能继续完成现有人脸识别、陌生人告警和推送流程。
- 连续运行后正常退出，无崩溃、死锁或明显资源泄漏。

### 11.3 验收条件

- 新增业务场景只需交付一个场景 `.so` 和一个 YAML，不修改主程序业务逻辑。
- 新增后处理能力只需实现统一接口、构建 `.so` 并加入场景配置。
- `FacePlugin` 不再包含目标跟踪实现。
- 跟踪开关完全由当前业务场景配置控制。
- 启动错误可诊断，单帧错误不破坏视频输出连续性。
- 本次不包含序列帧状态累计和最终汇总判断实现。
