#include "Engine.hpp"
#include <string.h>
#include "file.hpp"
#include "io.hpp"
#include "detection.hpp"
#include "Logger.h"

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

// int Engine::Postprocess(std::vector<detection::Object> &objects)
// {
// 	TIME_START(Postprocess);
// 	// 打印时间
// 	std::vector<float> anchors;
// 	for (size_t i = 0; i < m_config.anchors.size(); i++)
// 	{
// 		for (size_t j = 0; j < m_config.anchors[i].size(); j++)
// 		{
// 			anchors.push_back(m_config.anchors[i][j]);
// 		}
// 	}
	
// 	PostprocessYoloV5(m_pIoInfo, m_sIoData, 1920, 1080, m_config.inputs[2], m_config.inputs[3], m_config.prob_threshold, m_config.nms_threshold, anchors, m_config.strides, m_config.labels, objects);

// 	TIME_END(Postprocess);

// 	TIME_USEC_SHOW(Postprocess);
// 	return 0;
// }
/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */