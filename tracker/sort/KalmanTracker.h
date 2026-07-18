///////////////////////////////////////////////////////////////////////////////
// KalmanTracker.h: KalmanTracker Class Declaration

#ifndef KALMAN_H
#define KALMAN_H 2

#include "datatrans.h"
#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace cv;

#define StateType Rect_<float>

// This class represents the internel state of individual tracked objects observed as bounding box.
class KalmanTracker
{
public:
	
	KalmanTracker(TrackingBox init_track_box)
	{
		InitKf(init_track_box.box);  // 卡尔曼滤波器只需要bbox即可
		time_since_update = 0;  // // 创建新tracker时不会调用update函数，初始化为0
		observed_num = 1;  // 首次被检测模型观测到
		hit_streak = 0;
		age = 1;
		track_id_ = kkf_count_;
		kkf_count_++;

		obj_conf = init_track_box.obj_conf;
		class_id = init_track_box.class_id;
	}

	KalmanTracker(){}

	~KalmanTracker()
	{
		m_history.clear();
	}

	StateType Predict();
	void Update(TrackingBox track_box);

	StateType GetState();
	StateType GetRectXysr(float cx, float cy, float s, float r);

	StateType latest_rect;  // 当前跟踪序列最新的bbox坐标。在对外输出时：如果有观测值则使用观测值;如果没有观测值就使用预测值
	
	
	static int kkf_count_;  // 每次调用构造函数kf_count就会加1

	int max_missing_observed_num = 2;  // 允许连续跟丢的最大次数，比如101有效、1001有效、10001则无效
	int time_since_update;  // 距离上一次被观测到间隔的帧数
	int observed_num; // 该跟踪序列被检测模型观测到的总次数，如果某帧中没有检测到则值不会发生改变。每执行update一次，便hits+=1
	int hit_streak;	// 被连续观测到的次数。判断当前是否做了更新，大于等于1的说明做了更新，只要连续帧中没有做连续更新，hit_streak就会清零
	int age;			// 该目标框的年龄，初始化为1。每执行predict一次，便age+=1。可用于区别对待新生tracker和稳定tracker
	int track_id_;			// track_id。每次调用构造函数kf_count就会加1

	float obj_conf;
	int class_id;
	std::vector<float> kps_in_pic;
	std::vector<cv::Point2f> kps_in_robot;

private:
	void InitKf(StateType stateMat);

	cv::KalmanFilter kf;
	cv::Mat measurement;  // 观测值

	std::vector<StateType> m_history;  // 保存单个目标框连续预测的多个结果到history列表中，一旦执行update就会清空
};

#endif