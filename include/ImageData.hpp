#ifndef IMAGEDATA_HPP
#define IMAGEDATA_HPP

#include <iostream>
#include <string>
#include "FrameData.hpp"
#include <vector>
#include <opencv2/opencv.hpp>
struct ImageData
{
	AX_U32 u32Width;
	AX_U32 u32Height;
	AX_IMG_FORMAT_E enImgFormat;
	AX_BOOL bEndOfStream;
	std::shared_ptr<FrameData> data;
};
int Clone(ImageData &dest, ImageData const &src);
int JpegEncode(std::vector<uint8_t> &dest, ImageData const &src);
int Copy2Host(std::vector<uint8_t> &dest, ImageData const &src);
int Copy2Mat(cv::Mat &dest, ImageData const &src);
#endif /* ******************************************************* IMAGEDATA_H */