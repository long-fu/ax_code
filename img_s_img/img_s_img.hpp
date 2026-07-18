// #include "ALUtils.hpp"
// #include <FrameInfoProc.hpp>
// DH

// #include "common/inc/types.h"


#include <sys/stat.h>
#include <errno.h>
#include <string>
#include <cstdio>
#include <bits/stl_algo.h>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <uuid/uuid.h>
#include "qdrant_client.hpp"
#include "yolov5_ebm.hpp"
#include "Logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
static int RandomUUID(std::string &uuidStr)
{
    uuid_t uuid = {0x0};
    char uuid_str[37] = {0x0};
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    for (int i = 0, j = 0; i < 36; i++)
    {
        if (uuid_str[i] != '-')
        {
            uuid_str[j++] = uuid_str[i];
        }
    }
    uuid_str[32] = '\0';
    uuidStr = uuid_str;
    return 0;
}

#if 1

/// @brief 递归创建文件夹
/// @param path 路径 eg:"/home/workspace/img/a/aa"
/// @param mode 模式
/// @return 成功
static int mkdirs(const std::string &path, mode_t mode = 0755)
{
    for (size_t i = 0; i < path.size(); i++)
    {
        if (path[i] == '/')
        {
            std::string sub = path.substr(0, i);
            if (mkdir(sub.c_str(), mode) != 0 && errno != EEXIST)
            {

                return -1;
            }
        }
    }
    if (mkdir(path.c_str(), mode) != 0 && errno != EEXIST)
    {

        return -1;
    }
    return 0;
}

/// @brief 字符串格式化
/// @param fmt
/// @param
/// @return 字符串 format("hello %s, age: %d","world",20);
static std::string format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    std::string buf(len + 1, '\0');

    va_start(args, fmt);
    vsnprintf(&buf[0], len + 1, fmt, args);
    va_end(args);
    buf.resize(len);
    return buf;
}

/// @brief 移除字符串中的字数字符
/// @param str
/// @return
static std::string removeChar_(std::string &str)
{
    std::string tmp = str;
    tmp.erase(std::remove_if(str.begin(), tmp.end(),
                             [](char c)
                             {
                                 return c == ' ' || c == '-' || c == ':' || c == '.' || c == '+' || c == '/';
                             }),
              tmp.end());
    return tmp;
}


/**
 * 获取指定目录下的所有文件名（不包含子目录中的文件）
 * @param dirPath 目录路径
 * @return 文件名列表
 */
std::vector<std::string> getAllFilesInDirectory(const std::string &dirPath)
{
  std::vector<std::string> files;

  DIR *dir = opendir(dirPath.c_str());
  if (!dir)
  {
    std::cerr << "Error: Cannot open directory '" << dirPath << "'" << std::endl;
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr)
  {
    // 跳过 "." 和 ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
    {
      continue;
    }

    // 检查是否为普通文件（可选：排除目录）
    std::string fullPath = dirPath + "/" + entry->d_name;
    struct stat pathStat;
    if (stat(fullPath.c_str(), &pathStat) == 0)
    {
      if (S_ISREG(pathStat.st_mode))
      { // 是普通文件
        files.push_back(std::string(entry->d_name));
      }
    }
  }

  closedir(dir);
  return files;
}


#endif

class ImgSImg
{
public:
    struct Config
    {
        std::string model_path = "/home/workspace/deepsort/weights/VLT_GEN_person.axmodel";
        std::string qdrant_host = "http://22.10.57.58:6333";
        // std::string collection_type = "Scene"; // Scene , Event
        float score_threshold = 0.95;

        std::string channel_id = "1";
        std::string channel_name = "2";
        std::string scene_id = "test_fea";
        std::string device_ip = "2";
        std::string camera_ip = "2";
        std::string place_id = "1";
        std::string org_name = "1";

        std::vector<std::string> events_id = {"123"};
    };

    static int ReadConfig(std::string configPath, Config &config)
    {
        

        return 0;
    }

private:
    // 资源ID
    int channel_id_;

    Config m_Config = {};
    Yolov5Embedding m_Embedding;
    QdrantClient m_Client;

public:
    ImgSImg(int channelId, Config config) : m_Config(config),
                                            m_Embedding(config.model_path, channelId),
                                            m_Client(config.qdrant_host) {};
    ~ImgSImg() {};
    int Init()
    {
        int ret;
        ret = m_Embedding.Init();

        // m_ImgPath = "" +
        // m_ImgPath = format("%s/%s/%s",m_RootPath,)
        // ret = mkdirs(m_ImgPath);
        // 创建集合
        try
        {
           m_Client.delete_collection(m_Config.scene_id);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        
        
        for (size_t i = 0; i < m_Config.events_id.size(); i++)
        {
            // TODO: 可以按照配置读取
            try
            {
                VectorParams params;
                params.size = 400;
                params.vector_name = m_Config.events_id[i];
                params.distance = "Cosine";
                m_Client.create_collection(m_Config.scene_id, params);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            

        }
        return ret;
    }
    // 搜索, 返回UUID

    /// @brief 搜索是否存在相似图片
    /// @param id 数据id
    /// @param alarmTime 预警时间
    /// @param img 图片
    /// @return
    int Search(std::string &id,
               const std::string &alarmTime,
               ImageData &img,
               const std::string &imgPath,
               std::vector<std::string> events)
    {
        std::vector<float> vec;

        TIME_START(Search);

        TIME_START(Embedding);

        // 12 ms
        int ret = m_Embedding.Embedding(img, vec);
        TIME_END(Embedding);
        TIME_MSEC_SHOW(Embedding);

        LOG_INFO("vec size {}", vec.size());

        if (ret != 0)
        {
            return ret;
        }

        for (size_t i = 0; i < events.size(); i++)
        {
            std::string uuid = "";
            RandomUUID(uuid);
            id = uuid;

            float score = 0;
            std::string event_id = events[i];
            auto results = m_Client.query_points(m_Config.scene_id, vec, 1, event_id,0);
            
            if (results.size() > 0)
            {
                score = results[0].score;
            }
            
            LOG_INFO("query size: {} score:{}", results.size(), score);
            m_Config.score_threshold = 0.94;
            if (score >= m_Config.score_threshold)
            {
                LOG_INFO("找到相似图片:{}\n{}\n{}",score,imgPath, results[0].payload["path"].get<std::string>());
                std::remove(imgPath.c_str());
            }
            else
            {
                // 存图
                LOG_INFO("不存在相似图片,进行插入");
                std::map<std::string, json> payload;
                payload["id"] = uuid;
                payload["create_time"] = json(123);
                payload["path"] = imgPath;
                payload["alarm_time"] = alarmTime;
                payload["channel_id"] = m_Config.channel_id;
                payload["scene_id"] = m_Config.scene_id;
                payload["event_id"] = event_id;
                payload["device_ip"] = m_Config.device_ip;
                payload["place_id"] = m_Config.place_id;
                payload["cameraIP_ip"] = m_Config.camera_ip;
                payload["org_name"] = m_Config.org_name;

                std::vector<PointStruct> points = {
                    {json(uuid),
                     vec,
                     payload,
                     event_id}};

                m_Client.upsert_points(m_Config.scene_id, points);
            }
        }
        TIME_END(Search);
        TIME_MSEC_SHOW(Search);
        return 0;
    }
    // 更新
    int Update(const std::string &id)
    {
        return 0;
    }
};
