# TODO

- [ ] 侧脸特征可能是无效的。提取出来 不是陌生人可能也认为是陌生人。 (参数需要进行调整)

- [x] 可以把限制人脸 相似度搜索阈值降低 并且拿去前10条数据判断， 全是陌生人就是陌生人。 找到最近的时间点。做了目标跟随，会好很多，会大幅度减小这个问题。

- [ ] 判断是不是正向的人脸逻辑，方便做到最好的特征

- [ ] 直到track_id 都没有进行特征提取就是陌生人， 或者先持有相关数据，如果没有识别就直接推送报警。

- [ ] 拿到了比较正面的人脸 提取分析 是陌生人 就是陌生人
- [ ] 配置参数需要 实际调整

## 生产上线前必须处理（测试阶段暂缓）

- [ ] FaceServer API Key 目前作为 `face_server_client.h` 结构体默认值硬编码，导致
      `BusProcess::Init()` 里「key 未配置」检查永不触发。上线前改为：默认值置空、
      仅从环境变量/配置文件读取、缺失即 Init 失败。并轮换已泄漏的 key。
- [ ] Qdrant / FaceServer 目前是明文 HTTP 且 Qdrant 无鉴权，人脸图与特征向量在
      局域网内明文传输。上线前启用 HTTPS + Qdrant api-key。
- [ ] RTSP 账号密码硬编码在 `apps/main_rtsp.cpp`，上线前改为配置项。
- [ ] `test_bank_id` / `test_org_id` / `test_camera` / `stat_id` 等占位值需替换为真实配置。
- [ ] InfProcess 与 BusProcess 各自调用全局 `AX_ENGINE_Init` / `AX_ENGINE_Deinit`。
      重复 Init 实测无影响；但退出时 InfProcess 先析构会调用 `AX_ENGINE_Deinit`，
      而此时 BusProcess 的 ArcFace 句柄仍存在。目前只在退出阶段，影响可忽略，
      若后续日志出现退出期 NPU 报错，再改为全局单次引用计数 Init/Deinit。