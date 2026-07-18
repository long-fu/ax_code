#include <memory>
#include "img_s_img.hpp"
#include "ax_venc_api.h"
#include "yolov5.hpp"

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
};

int main(int argc, char const *argv[])
{

  LOG_INIT("logs/app.log", spdlog::level::debug);

  AX_INIT();

  // ImgSImg::Config config;

  // ImgSImg isi(0,config);

  IvpsHelper m_Ivps(0, 640 * 640 * 3, 16);

  Yolov5 yolov5("");

  int ret = yolov5.Init();
  if (ret != 0)
  {
    return -1;
  }

  ret = m_Ivps.Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640);
  if (ret != 0)
  {
    return ret;
  }

  std::string root = "/home/workspace/AF_fire";
  auto filenames = getAllFilesInDirectory(root);
  // list.clear();
  // std::string filenames;
  // std::vector<std::string> filenames;
  // filenames = {
  //   "AF_fire.2026-05-29 00:02:39.793825.430_05554-05554-22.45.64.87_05554_place_2.jpg",
  //   "AF_fire.2026-05-29 00:04:47.019851.866_05554-05554-22.45.64.87_05554_place_2.jpg"
  // };
  int fire_count = 0;
  for (size_t i = 0; i < filenames.size(); i++)
  {
    ImageData img;
    std::string file = root + "/" + filenames[i];

    JpegDecode(img, file);

    // size_t data_size = 640 * 640 * 3 / 2;
    ImageData resizeInfo;
    std::vector<uint8_t> data;
    int ret = m_Ivps.Process(resizeInfo, img);
    Copy2Host(data, resizeInfo);
    ret = yolov5.Process(data);
    std::vector<detection::Object> objects;
    yolov5.Postprocess(img.width, img.height, objects);

    LOG_INFO("---------------------------------");
    for (size_t i = 0; i < objects.size(); i++)
    {
      auto item = objects[i];
      LOG_INFO("box: {} {} {} {} {} {}", item.label, item.prob, item.rect.x, item.rect.y, item.rect.width, item.rect.height);
      if (item.label == 0)
      {
        fire_count++;
        break;
      }
    }

    // std::string uuid;
    // isi.Search(uuid,"time",img,file,{"123"});
  }

  LOG_INFO("fire image cout: {}", fire_count);
  LOG_INFO("Exit App");

  LOG_FLUSH();
  LOG_SHUTDOWN();
  return 0;
}
