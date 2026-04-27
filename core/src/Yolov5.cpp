#include "Yolov5.hpp"

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/




/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

Yolov5::~Yolov5()
{
}


/*
** --------------------------------- OVERLOAD ---------------------------------
*/


/*
** --------------------------------- METHODS ----------------------------------
*/

void Yolov5::Postprocess(int picWidth,int picHeight,
	std::vector<detection::Object> &objects)
{

	// TIME_START(Postprocess);
	// 打印时间
	EngineConfig config = GetConfig();
	std::vector<float> anchors;
	for (size_t i = 0; i < config.anchors.size(); i++)
	{
		for (size_t j = 0; j < config.anchors[i].size(); j++)
		{
			anchors.push_back(config.anchors[i][j]);
		}
	}
	
	// PostprocessYoloV5(m_pIoInfo, m_sIoData, 1920, 1080, m_config.inputs[2], m_config.inputs[3], m_config.prob_threshold, m_config.nms_threshold, anchors, m_config.strides, m_config.labels, objects);

	// TIME_END(Postprocess);

	// TIME_USEC_SHOW(Postprocess);
	// return 0;

	std::vector<detection::Object> proposals;

	// std::vector<float> anchors;
	std::vector<int> strides = config.strides;
	std::vector<std::string> labels = config.labels;

	float prob_threshold = config.prob_threshold;
	float nms_threshold = config.nms_threshold;
	int letterbox_cols = config.inputs[2];
	int letterbox_rows = config.inputs[3];

	int cls_num = labels.size();
	float prob_threshold_u_sigmoid = -1.0f * (float)std::log((1.0f / prob_threshold) - 1.0f);

	int src_rows = picHeight;
	int src_cols = picWidth;

	if (GetInfo()->nOutputSize != strides.size())
	{
		// LOG(ERROR) << "v5 det nOutputSize Error" << m_pInference->GetInfo()->nOutputSize;
		// LOG(ERROR) << "v5 det nOutputSize Error" << inferOutData.size;
		exit(-1);
	}

	for (uint32_t i = 0; i < GetInfo()->nOutputSize; ++i)
	{
		auto &output = GetOutput().pOutputs[i];
		auto ptr = (float *)output.pVirAddr;
		auto out_size = output.nSize;
		int32_t stride = strides[i];

		size_t countSize = (letterbox_cols / stride) * (letterbox_cols / stride) * (labels.size() + 5) * 3 * sizeof(float);

		if (countSize != out_size)
		{
			exit(-1);
		}

		detection::generate_proposals_yolov5(
			stride, i + 1, ptr, prob_threshold,
			proposals,
			letterbox_cols,
			letterbox_rows,
			anchors.data(),
			3,
			prob_threshold_u_sigmoid,
			cls_num);
	}

	detection::get_out_bbox(proposals, objects,
							nms_threshold,
							letterbox_rows,
							letterbox_cols,
							src_rows,
							src_cols);
}
/*
** --------------------------------- ACCESSOR ---------------------------------
*/


/* ************************************************************************** */