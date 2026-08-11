#include <memory>
#include "img_s_img.h"
#include "ax_venc_api.h"
#include "yolov5.h"

int AX_INIT()
{
  // MARK: 初始化系统资源

  int ret = 0;
  ret = AX_SYS_Init();
  if (AX_SUCCESS != ret)
  {
    LOG_ERROR("AX_SYS_Init Failed!! {:#}\n", ret);
    return ret;
  }

  // ret = AX_IVPS_Init();
  // if (AX_SUCCESS != ret)
  // {
  //     LOG_ERROR("AX_IVPS_Init Failed!! %X\n", ret);
  //     return ret;
  // }

  AX_VDEC_MOD_ATTR_T stModAttr;
  memset(&stModAttr, 0x0, sizeof(AX_VDEC_MOD_ATTR_T));

  stModAttr.enDecModule = AX_ENABLE_BOTH_VDEC_JDEC;
  stModAttr.u32MaxGroupCount = AX_VDEC_MAX_GRP_NUM;

  ret = AX_VDEC_Init(&stModAttr);
  if (AX_SUCCESS != ret)
  {
    return ret;
  }

  AX_VENC_MOD_ATTR_T stEncModAttr;
  memset(&stEncModAttr, 0x0, sizeof(AX_VENC_MOD_ATTR_T));
  stEncModAttr.enVencType = AX_VENC_MULTI_ENCODER;
  stEncModAttr.stModThdAttr.u32TotalThreadNum = 1;
  stEncModAttr.stModThdAttr.bExplicitSched = AX_FALSE;
  ret = AX_VENC_Init(&stEncModAttr);
  if (AX_SUCCESS != ret)
  {
    LOG_ERROR("AX_VENC_Init Failed!! {:#}\n", ret);
    return ret;
  }
  LOG_INFO("SYS INIT SUCCCESS !!!");
  return 0;
}

static void AX_DEINIT() {
  AX_VENC_Deinit();
  AX_VDEC_Deinit();
  AX_SYS_Deinit();
}

int main(int argc, char const *argv[])
{

  (void)argc;
  (void)argv;
  InitLogger("logs/app.log", spdlog::level::debug);

  if (AX_INIT() != 0) {
    LOG_ERROR("AX_INIT failed");
    LOG_FLUSH();
    LOG_SHUTDOWN();
    return -1;
  }

  // ImgSimg::Config config;

  // ImgSimg isi(0,config);

  IvpsHelper m_Ivps(0, 640 * 640 * 3, 16);

  Yolov5 yolov5("");

  int ret = yolov5.Init();
  if (ret != 0)
  {
    AX_DEINIT();
    return -1;
  }

  ret = m_Ivps.Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640);
  if (ret != 0)
  {
    AX_DEINIT();
    return ret;
  }

  std::string root = "/home/workspace/AF_fire";
  auto filenames = getAllFilesInDirectory(root);
  int fire_count = 0;
  for (size_t i = 0; i < filenames.size(); i++)
  {
    ImageData img;
    std::string file = root + "/" + filenames[i];

    if (JpegDecode(img, file) != 0) {
      LOG_ERROR("JpegDecode failed: {}", file);
      continue;
    }

    ImageData resizeInfo;
    std::vector<uint8_t> data;
    ret = m_Ivps.Process(resizeInfo, img);
    if (ret != 0) {
      LOG_ERROR("Ivps Process failed: {}", ret);
      continue;
    }
    Copy2Host(data, resizeInfo);
    ret = yolov5.Process(data);
    if (ret != 0) {
      LOG_ERROR("yolov5 Process failed: {}", ret);
      continue;
    }
    std::vector<detection::Object> objects;
    yolov5.Postprocess(img.width, img.height, objects);

    LOG_INFO("---------------------------------");
    for (size_t j = 0; j < objects.size(); j++)
    {
      auto item = objects[j];
      LOG_INFO("box: {} {} {} {} {} {}", item.label, item.prob, item.rect.x, item.rect.y, item.rect.width, item.rect.height);
      if (item.label == 0)
      {
        fire_count++;
        break;
      }
    }
  }

  LOG_INFO("fire image cout: {}", fire_count);
  LOG_INFO("Exit App");

  AX_DEINIT();
  LOG_FLUSH();
  LOG_SHUTDOWN();
  return 0;
}
