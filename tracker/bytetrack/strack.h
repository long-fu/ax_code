#pragma once

#include <opencv2/opencv.hpp>
#include "byte_kalman_filter.h"

enum TrackState { New = 0, Tracked, Lost, Removed };

class STrack
{
public:
	STrack( std::vector<float> tlwh_, float score);
	~STrack();

	 std::vector<float> static TlbrToTlwh( std::vector<float> &tlbr);
	void static MultiPredict( std::vector<STrack*> &stracks, byte_kalman::ByteKalmanFilter &kalman_filter);
	void StaticTlwh();
	void StaticTlbr();
	 std::vector<float> TlwhToXyah( std::vector<float> tlwh_tmp);
	 std::vector<float> ToXyah();
	void MarkLost();
	void MarkRemoved();
	int NextId();
	int EndFrame();
	
	void Activate(byte_kalman::ByteKalmanFilter &kalman_filter, int frame_id);
	void ReActivate(STrack &new_track, int frame_id, bool new_id = false);
	void Update(STrack &new_track, int frame_id);

public:
	bool is_activated;
	int track_id;
	int state;

	 std::vector<float> tlwh_internal_;
	 std::vector<float> tlwh;
	 std::vector<float> tlbr;
	int frame_id;
	int tracklet_len;
	int start_frame;

	KAL_MEAN mean;
	KAL_COVA covariance;
	float score;

private:
	byte_kalman::ByteKalmanFilter kalman_filter;
};