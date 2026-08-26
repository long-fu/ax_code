# TODO

- [ ] 侧脸特征可能是无效的。提取出来 不是陌生人可能也认为是陌生人。 (参数需要进行调整)

- [x] 可以把限制人脸 相似度搜索阈值降低 并且拿去前10条数据判断， 全是陌生人就是陌生人。 找到最近的时间点。做了目标跟随，会好很多，会大幅度减小这个问题。

- [ ] 判断是不是正向的人脸逻辑，方便做到最好的特征

- [ ] 直到track_id 都没有进行特征提取就是陌生人， 或者先持有相关数据，如果没有识别就直接推送报警。

- [ ] 拿到了比较正面的人脸 提取分析 是陌生人 就是陌生人
- [ ] 配置参数需要 实际调整

	
feat_done 在 Qdrant 检索成功之前置位（上次只修了一半）.removed_stracks 永不清空,7×24 运行持续增长.队列满时静默丢帧,SendMessage 返回值全部忽略.HTTP 响应体无上限,可被撑爆内存.	
next_id() 用函数内 static int,非线程安全.这几个问题修复了吗？死代码帮我创建一个目录存起来。

## 生产上线前必须处理（测试阶段暂缓）

- [ ] FaceServer API Key 目前作为 `face_server_client.h` 结构体默认值硬编码，导致
      `HostServices::Init()` 里「key 未配置」检查永不触发。上线前改为：默认值置空、
      仅从环境变量/配置文件读取、缺失即 Init 失败。并轮换已泄漏的 key。
- [ ] **人脸数据明文传输 + Qdrant 无鉴权**（测试阶段接受，生产必须调整）

      现状：
      - `qdrant_client.hpp:22` `use_https = false`，`plugin/host_services.cpp`
        的 `config.api_key` 仍是注释状态 → Qdrant REST 6333 端口无鉴权且明文。
      - `plugin/host_services.cpp` `fs_cfg.base_url = "http://192.168.137.112:8848"`
        → FaceServer 的整帧 JPEG 与人脸 ROI JPEG 走明文 multipart。

      暴露面：同网段任何人都能 dump 整个 `face_embeddings` collection、
      或删除/投毒向量库（投毒后陌生人会被误判为已登记人员，直接绕过报警）；
      抓包即可还原人脸图。人脸属敏感个人信息，且本项目是银行场景
      （payload 里有 bank_id / org_id），合规要求更高。

      上线前动作：
      1. Qdrant 开启 api-key 并放开 `config.api_key` 赋值，key 从环境变量读取。
      2. 两个服务都切 HTTPS。注意 `http_client.cpp` 目前**完全没设 SSL 选项**，
         libcurl 默认会校验证书（VERIFYPEER=1），但交叉编译到板子上大概率没有
         CA bundle，直接切 https 会报 "unable to get local issuer certificate"。
         需要显式设 `CURLOPT_CAINFO`（自签证书可直接指向该证书）并把证书随镜像下发。
         切勿用关闭 VERIFYPEER 的方式绕过。
      3. Qdrant 只监听内网地址 + 防火墙限制来源 IP，不要暴露 6333。
- [ ] RTSP 账号密码硬编码在 `apps/main_rtsp.cpp`，上线前改为配置项。
- [ ] `test_bank_id` / `test_org_id` / `test_camera` / `stat_id` 等占位值需替换为真实配置。
- [ ] InfProcess（SCRFD）与 HostServices（ArcFace）仍各自调用全局
      `AX_ENGINE_Init` / `AX_ENGINE_Deinit`。业务插件不再持有 NPU。
      重复 Init 实测无影响；但退出时 InfProcess 先析构会调用 `AX_ENGINE_Deinit`，
      而此时 HostServices 的 ArcFace 句柄仍存在。目前只在退出阶段，影响可忽略，
      若后续日志出现退出期 NPU 报错，再改为全局单次引用计数 Init/Deinit。