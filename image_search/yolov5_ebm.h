
// #include <cstring>
// #include "engine.h"
// #include "ivps_helper.h"
// #include "image_data.h"
// #include "logger.h"


// void SaveBinFile(const std::string &filename, const void *data, uint32_t size)
// {
//     FILE *outFileFp = fopen(filename.c_str(), "wb+");
//     if (outFileFp == nullptr)
//     {
//         LOG_ERROR("Save file %s failed for open error", filename.c_str());
//         return;
//     }
//     fwrite(data, 1, size, outFileFp);

//     fflush(outFileFp);
//     fclose(outFileFp);
// }

// class Yolov5Embedding : public Engine
// {
// private:
//     IvpsHelper ivps_;

// public:
//     Yolov5Embedding(std::string modelPath, int channelId) : Engine(modelPath), ivps_(channelId, 640 * 640 * 3, 16) {};

//     int Init() override
//     {
//         int ret = 0;
//         ret = Engine::Init();
//         if (ret != 0)
//         {
//             return ret;
//         }
//         ret = ivps_.Resize(AX_IVPS_ASPECT_RATIO_AUTO, 640, 640);
//         if (ret != 0)
//         {
//             return ret;
//         }
//         return 0;
//     }
//     int Destroy()
//     {
//         return 0;
//     };
//     ~Yolov5Embedding() {

//     };
//     int Postprocess(int pic_width, int pic_height,
//                     std::vector<detection::Object> &objects)
//     {

//         return 0;
//     }
//     int Embedding(ImageData img, std::vector<float> &vec)
//     {
//         size_t data_size = 640 * 640 * 3 / 2;
//         ImageData resizeInfo;
//         std::vector<uint8_t> data;
//         int ret = ivps_.Process(resizeInfo, img);
//         Copy2Host(data, resizeInfo);
//         // std::vector<uint8_t> jpeg;
//         // JpegEncode(jpeg, resizeInfo);
//         // SaveBinFile("test.jpg", jpeg.data(), jpeg.size());
//         ret = Process(data);
//         if (ret != 0)
//         {
//             return ret;
//         }
//         vec.clear();
//         vec = GetVec();

//         return 0;
//     }

// private:
//     std::vector<float> GetVec()
//     {
//         int batch = 1;
//         int C = 512;
//         int H = 20;
//         int W = 20;
//         LOG_INFO("nOutputSize {}", GetInfo()->nOutputSize);
//         for (uint32_t i = 0; i < GetInfo()->nOutputSize; ++i)
//         {
            
//             auto &output = GetOutput().pOutputs[i];
//             auto ptr = output.pVirAddr;
//             return sum_channels((float *)ptr, batch, C, H, W);
//         }
//         return {};
//     }

//     std::vector<float> sum_channels(
//         const float *feature,
//         int batch, int C, int H, int W)
//     {
//         std::vector<float> result(batch * H * W, 0.f);
//         for (int b = 0; b < batch; ++b)
//             for (int c = 0; c < C; ++c)
//                 for (int h = 0; h < H; ++h)
//                     for (int w = 0; w < W; ++w)
//                         result[b * H * W + h * W + w] +=
//                             feature[b * (C * H * W) + c * (H * W) + h * W + w];
//         return result;
//     }
// };
