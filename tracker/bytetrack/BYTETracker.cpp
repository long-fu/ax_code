#include "BYTETracker.h"

BYTETracker::BYTETracker(const BYTETrackerConfig& config)
{
	track_thresh = config.track_thresh;
	high_thresh = config.high_thresh;
	match_thresh = config.match_thresh;

	frame_id = 0;
	max_time_lost =
		int(config.frame_rate / 30.0 * config.track_buffer);
}

BYTETracker::BYTETracker(int frame_rate, int track_buffer)
	: BYTETracker(BYTETrackerConfig{frame_rate, track_buffer})
{
}

BYTETracker::~BYTETracker()
{
}

std::vector<STrack> BYTETracker::update(
	const std::vector<detection::Object>& objects)
{
	return update(objects, legacy_next_track_id);
}

 std::vector<STrack> BYTETracker::update(
	 const std::vector<detection::Object>& objects, int& next_track_id)
{

	////////////////// Step 1: Get detections //////////////////
	this->frame_id++;
	 std::vector<STrack> activated_stracks;
	 std::vector<STrack> refind_stracks;
	 std::vector<STrack> removed_stracks;
	 std::vector<STrack> lost_stracks;
	 std::vector<STrack> detections;
	 std::vector<STrack> detections_low;

	 std::vector<STrack> detections_cp;
	 std::vector<STrack> tracked_stracks_swap;
	 std::vector<STrack> resa, resb;
	 std::vector<STrack> output_stracks;

	 std::vector<STrack*> unconfirmed;
	 std::vector<STrack*> tracked_stracks;
	 std::vector<STrack*> strack_pool;
	 std::vector<STrack*> r_tracked_stracks;

	if (objects.size() > 0)
	{
		for (int i = 0; i < objects.size(); i++)
		{
			 std::vector<float> tlbr_;
			tlbr_.resize(4);
            tlbr_[0] = objects[i].rect.x;
            tlbr_[1] = objects[i].rect.y;
            tlbr_[2] = objects[i].rect.x + objects[i].rect.width;
            tlbr_[3] = objects[i].rect.y + objects[i].rect.height;

            float score = objects[i].prob;

			STrack strack(STrack::tlbr_to_tlwh(tlbr_), score);
			if (score >= track_thresh)
			{
				detections.push_back(strack);
			}
			else
			{
				detections_low.push_back(strack);
			}
			
		}
	}

	// Add newly detected tracklets to tracked_stracks
	for (int i = 0; i < this->tracked_stracks.size(); i++)
	{
		if (!this->tracked_stracks[i].is_activated)
			unconfirmed.push_back(&this->tracked_stracks[i]);
		else
			tracked_stracks.push_back(&this->tracked_stracks[i]);
	}

	////////////////// Step 2: First association, with IoU //////////////////
	strack_pool = joint_stracks(tracked_stracks, this->lost_stracks);
	STrack::multi_predict(strack_pool, this->kalman_filter);

	 std::vector< std::vector<float> > dists;
	int dist_size = 0, dist_size_size = 0;
	dists = iou_distance(strack_pool, detections, dist_size, dist_size_size);

	 std::vector< std::vector<int> > matches;
	 std::vector<int> u_track, u_detection;
	linear_assignment(dists, dist_size, dist_size_size, match_thresh, matches, u_track, u_detection);

	for (int i = 0; i < matches.size(); i++)
	{
		STrack *track = strack_pool[matches[i][0]];
		STrack *det = &detections[matches[i][1]];
		if (track->state == TrackState::Tracked)
		{
			track->update(*det, this->frame_id);
			activated_stracks.push_back(*track);
		}
		else
		{
			track->re_activate(*det, this->frame_id);
			refind_stracks.push_back(*track);
		}
	}

	////////////////// Step 3: Second association, using low score dets //////////////////
	for (int i = 0; i < u_detection.size(); i++)
	{
		detections_cp.push_back(detections[u_detection[i]]);
	}
	detections.clear();
	detections.assign(detections_low.begin(), detections_low.end());
	
	for (int i = 0; i < u_track.size(); i++)
	{
		if (strack_pool[u_track[i]]->state == TrackState::Tracked)
		{
			r_tracked_stracks.push_back(strack_pool[u_track[i]]);
		}
	}

	dists.clear();
	dists = iou_distance(r_tracked_stracks, detections, dist_size, dist_size_size);

	matches.clear();
	u_track.clear();
	u_detection.clear();
	linear_assignment(dists, dist_size, dist_size_size, 0.5, matches, u_track, u_detection);

	for (int i = 0; i < matches.size(); i++)
	{
		STrack *track = r_tracked_stracks[matches[i][0]];
		STrack *det = &detections[matches[i][1]];
		if (track->state == TrackState::Tracked)
		{
			track->update(*det, this->frame_id);
			activated_stracks.push_back(*track);
		}
		else
		{
			track->re_activate(*det, this->frame_id);
			refind_stracks.push_back(*track);
		}
	}

	for (int i = 0; i < u_track.size(); i++)
	{
		STrack *track = r_tracked_stracks[u_track[i]];
		if (track->state != TrackState::Lost)
		{
			track->mark_lost();
			lost_stracks.push_back(*track);
		}
	}

	// Deal with unconfirmed tracks, usually tracks with only one beginning frame
	detections.clear();
	detections.assign(detections_cp.begin(), detections_cp.end());

	dists.clear();
	dists = iou_distance(unconfirmed, detections, dist_size, dist_size_size);

	matches.clear();
	 std::vector<int> u_unconfirmed;
	u_detection.clear();
	linear_assignment(dists, dist_size, dist_size_size, 0.7, matches, u_unconfirmed, u_detection);

	for (int i = 0; i < matches.size(); i++)
	{
		unconfirmed[matches[i][0]]->update(detections[matches[i][1]], this->frame_id);
		activated_stracks.push_back(*unconfirmed[matches[i][0]]);
	}

	for (int i = 0; i < u_unconfirmed.size(); i++)
	{
		STrack *track = unconfirmed[u_unconfirmed[i]];
		track->mark_removed();
		removed_stracks.push_back(*track);
	}

	////////////////// Step 4: Init new stracks //////////////////
	for (int i = 0; i < u_detection.size(); i++)
	{
		STrack *track = &detections[u_detection[i]];
		if (track->score < this->high_thresh)
			continue;
		track->activate(this->kalman_filter, this->frame_id,
						next_track_id++);
		activated_stracks.push_back(*track);
	}

	////////////////// Step 5: Update state //////////////////
	for (int i = 0; i < this->lost_stracks.size(); i++)
	{
		if (this->frame_id - this->lost_stracks[i].end_frame() > this->max_time_lost)
		{
			this->lost_stracks[i].mark_removed();
			removed_stracks.push_back(this->lost_stracks[i]);
		}
	}
	
	for (int i = 0; i < this->tracked_stracks.size(); i++)
	{
		if (this->tracked_stracks[i].state == TrackState::Tracked)
		{
			tracked_stracks_swap.push_back(this->tracked_stracks[i]);
		}
	}
	this->tracked_stracks.clear();
	this->tracked_stracks.assign(tracked_stracks_swap.begin(), tracked_stracks_swap.end());

	this->tracked_stracks = joint_stracks(this->tracked_stracks, activated_stracks);
	this->tracked_stracks = joint_stracks(this->tracked_stracks, refind_stracks);

	//std::cout << activated_stracks.size() << std::endl;

	this->lost_stracks = sub_stracks(this->lost_stracks, this->tracked_stracks);
	for (int i = 0; i < lost_stracks.size(); i++)
	{
		this->lost_stracks.push_back(lost_stracks[i]);
	}

	// this->removed_stracks 只需保留"上一帧"的移除列表，上面这次 sub_stracks
	// 用完即失效。一帧足够的理由：轨迹在第 N 帧 mark_removed 时并不会立刻从
	// lost_stracks 摘掉（189-196 只标记不删除），靠第 N+1 帧走到这里才被 sub
	// 掉；而 198-204 按 state==Tracked 过滤后它也回不到 tracked_stracks。
	//
	// 此处有意与上游实现分歧（上游不 clear，只增不减），因为不 clear 有两个后果：
	//   1) 容器无界增长，且每帧都被 sub_stracks 整体遍历，内存与单帧 CPU 都随
	//      运行时长线性上涨——短时测试看不出，7x24 会逐渐掉帧；
	//   2) 更要紧的是误伤：Removed 轨在被摘掉前还会参与一帧关联(line 76)，若
	//      此时匹配上就会 re_activate 复活，但其 id 已永久留在本容器里。该轨
	//      日后再次 mark_lost 时会在这里被立刻删掉，彻底失去 max_time_lost 的
	//      宽限期，只能以新 id 重新出现（对上层表现为同一人重复报警）。
	this->lost_stracks = sub_stracks(this->lost_stracks, this->removed_stracks);
	this->removed_stracks.clear();
	for (int i = 0; i < removed_stracks.size(); i++)
	{
		this->removed_stracks.push_back(removed_stracks[i]);
	}
	
	remove_duplicate_stracks(resa, resb, this->tracked_stracks, this->lost_stracks);

	this->tracked_stracks.clear();
	this->tracked_stracks.assign(resa.begin(), resa.end());
	this->lost_stracks.clear();
	this->lost_stracks.assign(resb.begin(), resb.end());
	
	for (int i = 0; i < this->tracked_stracks.size(); i++)
	{
		if (this->tracked_stracks[i].is_activated)
		{
			output_stracks.push_back(this->tracked_stracks[i]);
		}
	}
	return output_stracks;
}
