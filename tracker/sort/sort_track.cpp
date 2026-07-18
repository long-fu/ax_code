#include "sort_track.h"
#include "Logger.h"

double SortTracker::GetIOU(Rect_<float> bb_test, Rect_<float> bb_gt)
{
    float intersection = (bb_test & bb_gt).area();
    float unionArea = bb_test.area() + bb_gt.area() - intersection;
    if (unionArea < DBL_EPSILON)
        return 0;
    return (double)(intersection / unionArea);
}


void SortTracker::Update(const vector<TrackingBox> &detFrameData)
{
    // total_frames++;
    frame_count++;

    // count running time using clock()
    start_time = getTickCount();

    // 初始化，the first frame met
    if (trackers.size() == 0)
    {
        // initialize kalman trackers using first detections.
        for (unsigned int i = 0; i < detFrameData.size(); i++)
        {
            KalmanTracker trk = KalmanTracker(detFrameData[i]);
            trackers.push_back(trk);
        }
        return;
    }

    ///////////////////////////////////////
    // 3.1. get predicted locations from existing trackers.
    predicted_boxes.clear();

    for (auto it = trackers.begin(); it != trackers.end();)
    {
        Rect_<float> pBox = (*it).Predict();
        if (pBox.x >= 0 && pBox.y >= 0)
        {
            predicted_boxes.push_back(pBox);
            it++;
        }
        else
        {
            it = trackers.erase(it);
            //cerr << "Box invalid at frame: " << frame_count << endl;
        }
    }

    ///////////////////////////////////////
    // 3.2. associate detections to tracked object (both represented as bounding boxes)
    // dets : detFrameData[fi]
    trk_num = predicted_boxes.size();
    det_num = detFrameData.size();

    iou_matrix.clear();
    iou_matrix.resize(trk_num, vector<double>(det_num, 0));
    // compute iou matrix as a distance matrix
    for (unsigned int i = 0; i < trk_num; i++)
    {
        for (unsigned int j = 0; j < det_num; j++)
        {
            // use 1-iou because the hungarian algorithm computes a minimum-cost assignment.
            iou_matrix[i][j] = 1 - GetIOU(predicted_boxes[i], detFrameData[j].box);
        }
    }

    // solve the assignment problem using hungarian algorithm.
    // the resulting assignment is [track(prediction) : detection], with len=preNum
    HungarianAlgorithm HungAlgo;
    assignment.clear();
    double cost_ = HungAlgo.Solve(iou_matrix, assignment);
    if (cost_ == -1.0)
    {
        LOG_ERROR("hungarian assignment error !");
    }

    // find matches, unmatched_detections and unmatched_predictions
    unmatched_trajectories.clear();
    unmatched_detections.clear();
    all_items.clear();
    matched_items.clear();

    if (det_num > trk_num) //	there are unmatched detections
    {
        for (unsigned int n = 0; n < det_num; n++)
            all_items.insert(n);

        for (unsigned int i = 0; i < trk_num; ++i)
            matched_items.insert(assignment[i]);
        // 找到没有配对上的检测框
        set_difference(all_items.begin(), all_items.end(),
                       matched_items.begin(), matched_items.end(),
                       insert_iterator<set<int>>(unmatched_detections, unmatched_detections.begin()));
    }
    else if (det_num < trk_num) // there are unmatched trajectory/predictions
    {
        for (unsigned int i = 0; i < trk_num; ++i)
            if (assignment[i] == -1) // unassigned label will be set as -1 in the assignment algorithm
                unmatched_trajectories.insert(i);
    }

    // filter out matched with low IOU
    matched_pairs.clear();
    for (unsigned int i = 0; i < trk_num; ++i)
    {
        if (assignment[i] == -1) // pass over invalid values
            continue;
        if (1 - iou_matrix[i][assignment[i]] < iou_threshold)
        {
            unmatched_trajectories.insert(i);
            unmatched_detections.insert(assignment[i]);
        }
        else
            matched_pairs.push_back(cv::Point(i, assignment[i]));
    }

    ///////////////////////////////////////
    // 3.3. updating trackers

    // 3.3.1，update matched trackers with assigned detections.
    // each prediction is corresponding to a tracker
    int detIdx, trkIdx;
    for (unsigned int i = 0; i < matched_pairs.size(); i++)
    {
        trkIdx = matched_pairs[i].x;
        detIdx = matched_pairs[i].y;
        trackers[trkIdx].Update(detFrameData[detIdx]);
    }

    // 3.3.2，create and initialise new trackers for unmatched detections
    for (auto umd : unmatched_detections)
    {
        // 创建新tracker时不会调用KalmanTracker的update函数
        KalmanTracker tracker = KalmanTracker(detFrameData[umd]);
        trackers.push_back(tracker);
    }
    
    // 3.3.3，更新未匹配上的跟踪序列
    // 由于之前已经单独调用过predict函数，此处直接用预测的结果进行tracker的跟踪
    // 在predict和update函数中都对latestRect的值进行了更新，此处不再单独操作

    // 3.3.4，remove dead tracklet
    for (auto it = trackers.begin(); it != trackers.end();)
    {
        // 移除情况1：稳定的tracker，连续丢失次数超过阈值max_lost_time
        // 移除情况2：才刚创建的tracker，就连续丢失超过阈值lower_max_lost_time
        if ((it->time_since_update > max_lost_time) || 
            (it->age == max_lost_time && it->time_since_update==lower_max_lost_time))
                it = trackers.erase(it);
        else{
            ++it;
        }
    }

    cycle_time = (double)(getTickCount() - start_time);
    total_time += cycle_time / getTickFrequency();
}


vector<TrackingBox> SortTracker::GetReport(){
    // get trackers' output
    frame_tracking_result.clear();
    for (auto it = trackers.begin(); it != trackers.end(); ++it)
    {
        // min_hits不设置为0是因为第一次检测到的目标不用跟踪，不能设大，一般就是1，表示如果连续两帧都检测到目标
        // int time_window = 1; // 表示连续预测的次数
        // if ((it->time_since_update < time_window) && it->hit_streak >= min_hits)
        if (it->observed_num >= min_hits)
        {
            TrackingBox res;
            res.box = it->latest_rect;  // 如果有观测值则使用观测值;如果没有观测值就使用预测值
            res.track_id = it->track_id_ + 1; // +1 as MOT benchmark requires positive
            res.frame_id = frame_count;
            res.obj_conf = it->obj_conf;
            res.class_id = it->class_id;
            frame_tracking_result.push_back(res);
        }
        else{
            // 
        }
        
    }
    return frame_tracking_result;
}