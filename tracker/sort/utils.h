#ifndef  UTILS_H
#define  UTILS_H
#include <iostream>
#include <fstream>  // 使用fstream读写文件
#include <opencv2/opencv.hpp>
#include "sort_track.h"


// 获取目录中所有文件的文件路径，可以指定文件类型
void GetFilePaths(std::string &folder, std::vector<cv::String> &filepaths, std::string postfix, bool sort_=true);
// 从行人真值TXT文件中读取检测结果，返回以frame_id为key的字典
void GetDetectResults(std::string &detfile, std::map<int, std::vector<BoundingBox>> &det_results);
// 字符串分割，返回vector，sep:分割符号。可应对如下情况："3,4,5" | "3,4,5," | ",3,4,5,"
void SplitString(std::string &str, std::vector<int> &out, char sep);

// 可视化检测结果
// void DrawPic(cv::Mat &img, std::string &savepath, const std::vector<BoundingBox> &results);
// 可视化跟踪结果
void DrawPic(cv::Mat &img, std::string savepath, const std::vector<TrackingBox> &results, SortTracker &tracker);
void DrawPic(cv::Mat &img, const std::vector<TrackingBox> &results, SortTracker &tracker);
#endif