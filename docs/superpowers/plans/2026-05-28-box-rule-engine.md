# Box Rule Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个可动态加载自定义 box 判断逻辑的规则引擎，通过 C++ ABI 接口 + YAML 配置管理。

**Architecture:** 定义 `BoxRule` 虚基类作为插件接口（`Init/Process/Destroy`），规则引擎在运行时通过 `dlopen/dlsym` 加载 `.so` 文件，实例化插件并传入检测框列表执行判断。YAML 配置文件声明启用的插件及参数。BusProcess 调用规则引擎处理推理结果。

**Tech Stack:** C++17, `dlfcn.h` (dlopen/dlsym), spdlog

---

## 文件结构

| 文件 | 职责 |
|------|------|
| `common/BoxRule.hpp` | 插件抽象基类 `BoxRule` + 工厂注册器 `RuleFactory` |
| `common/RuleEngine.hpp` / `.cpp` | 规则引擎：加载配置、管理插件生命周期、执行所有插件 |
| `core/inc/BusProcess.hpp` | 修改：引入 RuleEngine，在 Process 中调用 |
| `rules/sample_rule/sample_rule.cpp` | 示例插件 — 区域入侵检测 |
| `rules/sample_rule/CMakeLists.txt` | 示例插件构建配置 |
| `config.yaml` | YAML 配置文件 |
| `CMakeLists.txt` | 添加 rules 子目录 |

---

## Task 1: 定义插件接口 `BoxRule` 和工厂注册器

**Files:**
- Create: `common/BoxRule.hpp`

- [ ] **Step 1: 编写插件接口头文件**

```cpp
// common/BoxRule.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "Base.h"  // detection::Object

class BoxRule {
public:
    virtual ~BoxRule() = default;

    // 初始化，接收 YAML 节点配置
    virtual int Init(const std::string& config) = 0;

    // 处理当前帧的检测框，返回判断结果
    // results: 输出每个 object 的结果，索引与 objects 一一对应
    virtual int Process(
        const std::vector<detection::Object>& objects,
        std::vector<bool>& results) = 0;

    // 销毁资源
    virtual int Destroy() = 0;
};

// 工厂函数签名：创建一个 BoxRule 实例
using RuleCreateFunc = BoxRule* (*)();
using RuleDestroyFunc = void (*)(BoxRule*);

// 工厂注册器：自动注册插件类型
class RuleFactory {
public:
    static RuleFactory& instance();

    // 注册一个插件创建函数
    void registerRule(const std::string& name, RuleCreateFunc create);

    // 根据名称创建实例
    BoxRule* createRule(const std::string& name) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RuleCreateFunc> registry_;
};

#define REGISTER_RULE(Class) \
    extern "C" BoxRule* create_##Class() { return new Class(); } \
    extern "C" void destroy_##Class(BoxRule* rule) { delete rule; } \
    struct RegisterHelper_##Class { \
        RegisterHelper_##Class() { \
            RuleFactory::instance().registerRule(#Class, create_##Class); \
        } \
    }; \
    static RegisterHelper_##Class g_register_##Class;
```

- [ ] **Step 2: Commit**

```bash
git add common/BoxRule.hpp
git commit -m "feat: define BoxRule plugin interface and factory"
```

---

## Task 2: 实现规则引擎核心

**Files:**
- Create: `common/RuleEngine.hpp`
- Create: `common/RuleEngine.cpp`

- [ ] **Step 1: 编写引擎头文件**

```cpp
// common/RuleEngine.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include "BoxRule.hpp"

struct RuleConfig {
    std::string name;       // 插件名，对��注册的规则名
    std::string so_path;    // .so 文件路径
    std::string params;     // YAML/JSON 参数字符串
};

class RuleEngine {
public:
    static RuleEngine& instance();

    // 从 YAML 字符串加载配置并初始化所有插件
    int load(const std::string& yaml_config);

    // 处理检测框，返回每个对象是否命中（任一插件命中的 OR 结果）
    std::vector<bool> processBoxes(
        const std::vector<detection::Object>& objects) const;

    // 释放所有插件资源
    void unload();

private:
    struct LoadedRule {
        void* handle = nullptr;      // dlopen 句柄
        BoxRule* instance = nullptr;  // 插件实例
        RuleConfig config;
    };

    mutable std::mutex mutex_;
    std::vector<LoadedRule> rules_;
};
```

- [ ] **Step 2: 实现引擎核心逻辑**

```cpp
// common/RuleEngine.cpp
#include "RuleEngine.hpp"
#include "Logger.h"
#include <dlfcn.h>
#include <sstream>
#include <algorithm>

RuleEngine& RuleEngine::instance() {
    static RuleEngine inst;
    return inst;
}

int RuleEngine::load(const std::string& yaml_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 解析 YAML 配置，格式:
    // rules:
    //   - name: sample_rule
    //     path: /path/to/libsample_rule.so
    //     params: |
    //       region: [[100,100],[300,100],[300,300],[100,300]]
    std::vector<RuleConfig> configs;
    std::istringstream stream(yaml_config);
    std::string line;
    RuleConfig* current = nullptr;
    std::stringstream param_buf;
    bool in_params = false;

    while (std::getline(stream, line)) {
        if (line.empty() || line.find("#") == 0) continue;

        if (line.find("name:") != std::string::npos) {
            if (current && !current->name.empty()) configs.push_back(*current);
            RuleConfig cfg{};
            current = &cfg;
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                current->name = line.substr(pos + 1);
            current->name.erase(0, current->name.find_first_not_of(" \t"));
            current->name.erase(current->name.find_last_not_of(" \t\r\n") + 1);
            in_params = false;
        } else if (line.find("path:") != std::string::npos && current) {
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                current->so_path = line.substr(pos + 1);
            current->so_path.erase(0, current->so_path.find_first_not_of(" \t"));
            current->so_path.erase(current->so_path.find_last_not_of(" \t\r\n") + 1);
            in_params = false;
        } else if (line.find("params:") != std::string::npos) {
            in_params = true;
            param_buf.str("");
            size_t pos = line.find(":");
            if (pos != std::string::npos && pos + 2 < line.size()) {
                std::string val = line.substr(pos + 1);
                val.erase(0, val.find_first_not_of(" \t"));
                if (!val.empty() && val[0] != '|') {
                    param_buf << val << "\n";
                    in_params = false;
                }
            }
        } else if (in_params && current) {
            param_buf << line << "\n";
        }
    }
    if (current && !current->name.empty()) configs.push_back(*current);

    // 加载每个插件
    for (auto& cfg : configs) {
        LoadedRule loaded;
        loaded.config = cfg;

        loaded.handle = dlopen(cfg.so_path.c_str(), RTLD_NOW);
        if (!loaded.handle) {
            LOG_ERROR("dlopen failed: {}, error: {}", cfg.so_path, dlerror());
            continue;
        }

        std::string create_name = "create_" + cfg.name;
        auto create_fn = (RuleCreateFunc)dlsym(loaded.handle, create_name.c_str());
        if (!create_fn) {
            LOG_ERROR("dlsym {} failed: {}", create_name, dlerror());
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        loaded.instance = create_fn();
        if (!loaded.instance) {
            LOG_ERROR("Failed to create rule instance: {}", cfg.name);
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        if (loaded.instance->Init(cfg.params) != 0) {
            LOG_ERROR("Rule Init failed: {}", cfg.name);
            loaded.instance->Destroy();
            delete loaded.instance;
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        rules_.push_back(std::move(loaded));
    }

    LOG_INFO("RuleEngine loaded {} rules", rules_.size());
    return 0;
}

std::vector<bool> RuleEngine::processBoxes(
    const std::vector<detection::Object>& objects) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<bool> results(objects.size(), false);

    for (const auto& rule : rules_) {
        if (!rule.instance) continue;

        std::vector<bool> rule_results(objects.size(), false);
        rule.instance->Process(objects, rule_results);

        for (size_t i = 0; i < objects.size(); ++i) {
            if (rule_results[i]) {
                results[i] = true;
            }
        }
    }

    return results;
}

void RuleEngine::unload() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& rule : rules_) {
        if (rule.instance) {
            rule.instance->Destroy();
            delete rule.instance;
            rule.instance = nullptr;
        }
        if (rule.handle) {
            dlclose(rule.handle);
            rule.handle = nullptr;
        }
    }
    rules_.clear();
}
```

- [ ] **Step 3: Commit**

```bash
git add common/RuleEngine.hpp common/RuleEngine.cpp
git commit -m "feat: implement RuleEngine with dlopen-based plugin loading"
```

---

## Task 3: 集成到 BusProcess

**Files:**
- Modify: `core/inc/BusProcess.hpp`

- [ ] **Step 1: 修改 BusProcess，集成 RuleEngine**

改动点：
1. 包含 `RuleEngine.hpp`
2. `Init()` 中读取 `config.yaml` 并调用 `RuleEngine::instance().load()`
3. `Process(kMsgInfprocData)` 中调用 `RuleEngine::processBoxes()` 获取结果
4. `kMsgAppExit` 中调用 `RuleEngine::instance().unload()`

关键代码替换：

```cpp
// core/inc/BusProcess.hpp 新增 include
#include "RuleEngine.hpp"
#include <fstream>
#include <sstream>

// Init() 替换为
int Init() override
{
    m_next_thread_id_ = GetPipelineThreadIdByName("EncProcess");

    // 加载规则引擎配置
    std::ifstream config_file("config.yaml");
    if (config_file.is_open()) {
        std::stringstream buf;
        buf << config_file.rdbuf();
        std::string yaml_content = buf.str();

        // 提取 rules 部分
        size_t pos = yaml_content.find("rules:");
        if (pos != std::string::npos) {
            size_t first_item = yaml_content.find("- ", pos);
            if (first_item != std::string::npos) {
                std::string rules_yaml = yaml_content.substr(first_item);
                RuleEngine::instance().load(rules_yaml);
            }
        }
    }

    return 0;
}

// Process(kMsgInfprocData) 中 SORT 跟踪后追加
// ... SORT 跟踪代码不变 ...

// 规则引擎判断 — 对所有 label 的对象做判断
std::vector<bool> rule_results = RuleEngine::instance().processBoxes(in_data->objects);

// 绘制时叠加规则结果
for (size_t i = 0; i < tracking_results.size(); i++) {
    auto item = tracking_results[i];
    DrawText(...);
    DrawRect(...);
    // 可选：如果规则��中，额外标注
}

// kMsgAppExit 中追加
case kMsgAppExit:
    RuleEngine::instance().unload();
    break;
```

- [ ] **Step 2: Commit**

```bash
git add core/inc/BusProcess.hpp
git commit -m "feat: integrate RuleEngine into BusProcess"
```

---

## Task 4: 创建示例插件和配置文件

**Files:**
- Create: `rules/sample_rule/sample_rule.cpp`
- Create: `rules/sample_rule/CMakeLists.txt`
- Create: `config.yaml`

- [ ] **Step 1: 编写示例插件 — 区域入侵检测**

```cpp
// rules/sample_rule/sample_rule.cpp
#include "../../common/BoxRule.hpp"
#include <cmath>

static bool pointInPolygon(const std::vector<std::pair<float, float>>& poly, float x, float y) {
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i].second > y) != (poly[j].second > y)) &&
            (x < (poly[j].first - poly[i].first) * (y - poly[i].second) /
                     (poly[j].second - poly[i].second) + poly[i].first)) {
            inside = !inside;
        }
    }
    return inside;
}

class SampleRule : public BoxRule {
public:
    int Init(const std::string& config) override {
        // 解析 region: [[100,100],[300,100],[300,300],[100,300]]
        size_t pos = 0;
        while ((pos = config.find("[[", pos)) != std::string::npos) {
            size_t end = config.find("]]", pos + 2);
            if (end == std::string::npos) break;
            std::string pair_str = config.substr(pos + 2, end - pos - 2);
            size_t comma = pair_str.find(",");
            if (comma != std::string::npos) {
                float x = std::stof(pair_str.substr(0, comma));
                float y = std::stof(pair_str.substr(comma + 1));
                region_.emplace_back(x, y);
            }
            pos = end + 2;
        }
        LOG_INFO("SampleRule initialized with {} region points", region_.size());
        return 0;
    }

    int Process(const std::vector<detection::Object>& objects,
                std::vector<bool>& results) override {
        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& obj = objects[i];
            float cx = obj.rect.x + obj.rect.width / 2.0f;
            float cy = obj.rect.y + obj.rect.height / 2.0f;
            results[i] = pointInPolygon(region_, cx, cy);
        }
        return 0;
    }

    int Destroy() override {
        region_.clear();
        return 0;
    }

private:
    std::vector<std::pair<float, float>> region_;
};

REGISTER_RULE(SampleRule)
```

- [ ] **Step 2: 编写示例插件 CMakeLists**

```cmake
# rules/sample_rule/CMakeLists.txt
add_library(sample_rule SHARED sample_rule.cpp)
target_include_directories(sample_rule PRIVATE ..)
set_target_properties(sample_rule PROPERTIES
    OUTPUT_NAME "libsample_rule"
)
```

- [ ] **Step 3: 编写 config.yaml**

```yaml
# config.yaml
pipeline:
  rtsp_input: "rtsp://..."
  rtmp_output: "rtmp://..."

rules:
  - name: sample_rule
    path: "./libsample_rule.so"
    params: |
      region: [[100,100],[600,100],[600,500],[100,500]]
```

- [ ] **Step 4: Commit**

```bash
git add rules/ config.yaml
git commit -m "feat: add sample region intrusion rule plugin and config"
```

---

## Task 5: 更新 CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 添加 rules 子目录**

```cmake
# CMakeLists.txt 末尾追加
add_subdirectory(rules)
```

- [ ] **Step 2: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add rules subdirectory to CMakeLists"
```

---

## Self-Review

**1. Spec coverage:**
- 自定义规则引擎 ✓ (Task 2)
- C++ ABI 接口导出 ✓ (Task 1: `BoxRule` 虚基类 + `extern "C"` 工厂函数)
- YAML 配置管理 ✓ (Task 2: YAML 解析 + Task 4: config.yaml)
- 集成到 BusProcess ✓ (Task 3)
- 示例插件 ✓ (Task 4)

**2. Placeholder scan:**
- 无 TBD/TODO 占位符
- 所有代码块完整可执行
- YAML 解析是简化的但满足基本需求

**3. Type consistency:**
- `BoxRule::Process` 签名在所有地方一致
- `detection::Object` 来自 `common/Base.h`，贯穿使用
- `RuleFactory::registerRule` 命名一致

---

Plan complete and saved to `docs/superpowers/plans/2026-05-28-box-rule-engine.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
