#include "utils.h"

void GetFilePaths(std::string &folder, std::vector<cv::String> &filepaths, std::string postfix, bool sort_){
    // 获取目录中某种格式的文件，返回其路径
    
    // 1，根据后缀进行过滤
    std::vector<cv::String> res;
    cv::glob(folder, res);  // 返回值是路径
    for (auto filepath:res){
        // 获取字符串文件名的后缀
        if (filepath.substr(filepath.find_last_of('.') + 1) == postfix){
            filepaths.push_back(filepath);
        }
    }
    // 2，排序
    if (sort_){
        std::sort(filepaths.begin(), filepaths.end());
    }
}

void GetDetectResults(std::string &detfile, std::map<int, std::vector<BoundingBox>> &det_results){
    std::ifstream fin;
    fin.open(detfile, std::ios::in);
    std::string str_line;
    // 逐行读取
    while (std::getline(fin, str_line))
    {
        std::vector<int> res;  // 当前行按逗号分割后的结果
        SplitString(str_line, res, ',');
        // 解析bbox
        int frame_id = res[0];
        BoundingBox bbox;
        bbox.bbox_id = res[1];
        bbox.rect = cv::Rect_<int>(res[2], res[3], res[4], res[5]);
        // 判断map中某个key是否存在，如果不存在需要先初始化
        if (det_results.find(frame_id) == det_results.end()){
            det_results[frame_id] = std::vector<BoundingBox>();  // 初始化为空值
        }
        det_results[frame_id].push_back(bbox);
    }
}

void SplitString(std::string &str, std::vector<int> &out, char sep)
{
    std::string::size_type start = str.find_first_not_of(sep, 0); // 找到第一个不为逗号的下标
    std::string::size_type pose = str.find_first_of(sep, start);  // 找到第一个逗号的下标
    while (std::string::npos != start || std::string::npos != pose)
    { // 当即没有逗号也没有字符的时候结束
        out.push_back(std::atoi(str.substr(start, pose - start).c_str()));  // 字符串转int
        start = str.find_first_not_of(sep, pose); // 更新start 从pose开始
        pose = str.find_first_of(sep, start);     // 更新pos,从start开始
    }
}

// void DrawPic(cv::Mat &img, string savepath, const std::vector<BoundingBox> &results){
//     vector<cv::Scalar> colors = {{0,0,255}, {0,255,0}, {255,0,0}, {255,255,0}, {0,255,255}, {255,0,255}, {255,153,18}, {255,97,0}};
//     for (auto obj:results){
//         // cv::Scalar color = {rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)};
//         cv::Scalar color = colors[obj.class_id];
//         // bbox
//         cv::rectangle(img, obj.rect, color, 2);
//         // label
//         int score_ = int(obj.score * 100);
//         string label = to_string(obj.class_id) + "-" + to_string(score_);
//         cv::putText(img, label, {obj.rect.x, obj.rect.y-3}, 0, 0.4, color, 1, 16);
//     }
//     cv::imwrite(savepath, img);
//     cout << "save vis img in: " << savepath << endl;
// }

void DrawPic(cv::Mat &img, std::string savepath, const std::vector<TrackingBox> &results, SortTracker &tracker){
    for (TrackingBox it : results)
    {
        cv::rectangle(img, it.box, tracker.rand_color[it.track_id % 255], 2);
        cv::putText(img,
                    std::to_string(it.track_id),
                    cv::Point2f(it.box.x, it.box.y-5),
                    cv::FONT_HERSHEY_DUPLEX,
                    1,
                    tracker.rand_color[it.track_id % 255]);
    }
    cv::imwrite(savepath, img);
}

void DrawPic(cv::Mat &img, const std::vector<TrackingBox> &results, SortTracker &tracker){
    for (TrackingBox it : results)
    {
        cv::rectangle(img, it.box, tracker.rand_color[it.track_id % 255], 2);
        cv::putText(img,
                    std::to_string(it.track_id),
                    cv::Point2f(it.box.x, it.box.y-5),
                    cv::FONT_HERSHEY_DUPLEX,
                    1,
                    tracker.rand_color[it.track_id % 255]);
    }
}