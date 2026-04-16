#ifndef INFERENCESESSION_HPP
#define INFERENCESESSION_HPP

#include <iostream>
#include <string>
#include <ax_sys_api.h>
#include <ax_engine_api.h>
#include <vector>
#include "detection.hpp"
struct EngineConfig
{

	// int letterbox_cols;
	// int letterbox_rows;
	std::string model_file = "./ALL_GEN_person.axmodel";
	std::string model_type = "yolov5"; 
	std::vector<int> inputs = {1, 3, 640, 640};

	float prob_threshold = 0.65;
	float nms_threshold = 0.45;

	std::vector<int> num_anchors = {3, 3, 3};
	std::vector<std::vector<float>> anchors = {{10, 13}, {16, 30}, {33, 23}, {30, 61}, {62, 45}, {59, 119}, {116, 90}, {156, 198}, {373, 326}};
	std::vector<int> strides = {8, 16, 32};

	std::vector<std::string> labels = {"head","person"};

	std::string configPath;
	EngineConfig(std::string path)
	{
		configPath = path;
	}
	int Init()
	{
		return 0;
	}
};

class Engine
{

public:
	Engine(std::string modelConfig);

	~Engine();

	int Init();

	int Destroy();

	int Process(const std::vector<uint8_t> &data);
	int Process(const uint8_t *data, size_t size);
	// int Postprocess(std::vector<detection::Object> &objects);
	virtual void Postprocess(int picWidth,int picHeight,std::vector<detection::Object> &objects) = 0;
	AX_ENGINE_IO_T GetOutput() const
	{
		return m_sIoData;
	}

	AX_ENGINE_IO_INFO_T *GetInfo() const
	{
		return m_pIoInfo;
	}

	EngineConfig GetConfig() const {
		return m_config;
	}

private:
	// std::string m_modelPath;
	AX_ENGINE_HANDLE m_tHandle;
	AX_ENGINE_IO_INFO_T *m_pIoInfo;
	AX_ENGINE_IO_T m_sIoData;
	bool m_isReleased = false;
	uint32_t m_nModelWidth = 640;
	uint32_t m_nModelHeight = 640;
	EngineConfig m_config;
};

#endif /* ************************************************ INFERENCESESSION_H */