#include "Engine.hpp"
#include <string.h>
#include "file.hpp"
#include "io.hpp"
#include "detection.hpp"
#include "Logger.h"
void PostprocessYoloV5(
	const AX_ENGINE_IO_INFO_T *ioInfo,
	const AX_ENGINE_IO_T ioData,

	int picHeight,
	int picWidth,

	int letterbox_cols,
	int letterbox_rows,

	float prob_threshold,
	float nms_threshold,

	std::vector<float> anchors,
	std::vector<int> strides,
	std::vector<std::string> labels,

	std::vector<detection::Object> &objects)
{
	std::vector<detection::Object> proposals;

	// std::vector<float> anchors;
	// std::vector<int> strides;
	// std::vector<std::string> labels;

	// float prob_threshold = 0.25;
	// float nms_threshold = 0.25;
	// int letterbox_cols = 640;
	// int letterbox_rows = 640;

	int cls_num = labels.size();
	float prob_threshold_u_sigmoid = -1.0f * (float)std::log((1.0f / prob_threshold) - 1.0f);

	int src_rows = picHeight;
	int src_cols = picWidth;

	if (ioInfo->nOutputSize != strides.size())
	{
		// LOG(ERROR) << "v5 det nOutputSize Error" << m_pInference->GetInfo()->nOutputSize;
		// LOG(ERROR) << "v5 det nOutputSize Error" << inferOutData.size;
		exit(-1);
	}

	for (uint32_t i = 0; i < ioInfo->nOutputSize; ++i)
	{
		auto &output = ioData.pOutputs[i];
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
** ------------------------------- CONSTRUCTOR --------------------------------
*/

Engine::Engine(std::string modelConfig) : m_config(modelConfig)
{
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

Engine::~Engine()
{
	Destroy();
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

/*
** --------------------------------- METHODS ----------------------------------
*/
int Engine::Init()
{
	m_config.Init();

	// 1. init engine
	AX_ENGINE_NPU_ATTR_T npu_attr;
	memset(&npu_attr, 0, sizeof(npu_attr));
	npu_attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_STD;
	auto ret = AX_ENGINE_Init(&npu_attr);
	if (0 != ret)
	{
		// LOG(ERROR) << "AX_ENGINE_Init failed!!!";
		return ret;
	}
	// std::cout << "Load Model Path: " << m_sModelPath << std::endl;
	// 2. load model
	std::vector<char> model_buffer;
	if (!utilities::read_file(m_config.model_file, model_buffer))
	{
		// LOG(ERROR) << "Read Run-Joint model file failed. file: " << m_sModelPath;
		return false;
	}

	// 3. create handle
	ret = AX_ENGINE_CreateHandle(&m_tHandle, model_buffer.data(), model_buffer.size());
	if (0 != ret)
	{
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	// LOG(INFO) << "Engine creating handle is done.";
	// 4. create context
	ret = AX_ENGINE_CreateContext(m_tHandle);
	if (0 != ret)
	{
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	// LOG(INFO) << "Engine creating context is done.";
	// 5. set io
	ret = AX_ENGINE_GetIOInfo(m_tHandle, &m_pIoInfo);
	if (0 != ret)
	{
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	// LOG(INFO) << "Engine get io info is done.";
	// 6. alloc io
	ret = middleware::prepare_io(m_pIoInfo, &m_sIoData, std::make_pair(AX_ENGINE_ABST_DEFAULT, AX_ENGINE_ABST_CACHED));
	if (0 != ret)
	{
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	// LOG(INFO) << "Engine alloc io is done.";
	return 0;
}

int Engine::Process(const std::vector<uint8_t> &input_data)
{
	TIME_START(EngineProcess);
	// 7. insert input
	int ret = middleware::push_input(input_data, &m_sIoData, m_pIoInfo);
	if (0 != ret)
	{
		// LOG(ERROR) << "push_input data failed!!!";
		middleware::free_io(&m_sIoData);
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	// 8. warn up
	// 9. run model
	ret = AX_ENGINE_RunSync(m_tHandle, &m_sIoData);
	if (0 != ret)
	{
		// LOG(ERROR) << "AX_ENGINE_RunSync failed!!!";
		middleware::free_io(&m_sIoData);
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}

	TIME_END(EngineProcess);

	TIME_USEC_SHOW(EngineProcess);
	return ret;
}

int Engine::Process(const uint8_t *data, size_t size)
{
	// 7. insert input
	int ret = middleware::push_input(data, size, &m_sIoData, m_pIoInfo);
	if (0 != ret)
	{
		// LOG(ERROR) << "push_input data failed!!!";
		middleware::free_io(&m_sIoData);
		AX_ENGINE_DestroyHandle(m_tHandle);
		// 考虑推出
		return ret;
	}
	// 8. warn up
	// 9. run model
	ret = AX_ENGINE_RunSync(m_tHandle, &m_sIoData);
	if (0 != ret)
	{
		// LOG(ERROR) << "AX_ENGINE_RunSync failed!!!";
		middleware::free_io(&m_sIoData);
		AX_ENGINE_DestroyHandle(m_tHandle);
		return ret;
	}
	return ret;
}

int Engine::Destroy()
{
	if (m_isReleased == true)
	{
		// LOG_INFO("isReleased");
		return 0;
	}
	m_isReleased = true;

	middleware::free_io(&m_sIoData);
	AX_S32 ret = AX_ENGINE_DestroyHandle(m_tHandle);

	AX_ENGINE_Deinit();

	return ret;
}

int Engine::Postprocess(std::vector<detection::Object> &objects)
{
	TIME_START(Postprocess);
	// 打印时间
	std::vector<float> anchors;
	for (size_t i = 0; i < m_config.anchors.size(); i++)
	{
		for (size_t j = 0; j < m_config.anchors[i].size(); j++)
		{
			anchors.push_back(m_config.anchors[i][j]);
		}
	}
	
	PostprocessYoloV5(m_pIoInfo, m_sIoData, 1920, 1080, m_config.inputs[2], m_config.inputs[3], m_config.prob_threshold, m_config.nms_threshold, anchors, m_config.strides, m_config.labels, objects);

	TIME_END(Postprocess);

	TIME_USEC_SHOW(Postprocess);
	return 0;
}
/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */