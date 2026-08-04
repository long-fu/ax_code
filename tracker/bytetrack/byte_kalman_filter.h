#pragma once

#include "dataType.h"

namespace byte_kalman
{
	class ByteKalmanFilter
	{
	public:
		static const double chi2inv95[10];
		ByteKalmanFilter();
		KAL_DATA Initiate(const DETECTBOX& measurement);
		void Predict(KAL_MEAN& mean, KAL_COVA& covariance);
		KAL_HDATA Project(const KAL_MEAN& mean, const KAL_COVA& covariance);
		KAL_DATA Update(const KAL_MEAN& mean,
			const KAL_COVA& covariance,
			const DETECTBOX& measurement);

		Eigen::Matrix<float, 1, -1> GatingDistance(
			const KAL_MEAN& mean,
			const KAL_COVA& covariance,
			const std::vector<DETECTBOX>& measurements,
			bool only_position = false);

	private:
		Eigen::Matrix<float, 8, 8, Eigen::RowMajor> motion_mat_;
		Eigen::Matrix<float, 4, 8, Eigen::RowMajor> update_mat_;
		float std_weight_position_;
		float std_weight_velocity_;
	};
}