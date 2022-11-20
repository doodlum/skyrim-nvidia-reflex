#include "Reflex.h"

#include <SimpleIni.h>

#define MAGIC_ENUM_RANGE_MAX 256
#include <magic_enum.hpp>

#include <ENB/ENBSeriesAPI.h>

// Exports

EXTERN_C bool GetReflexEnabled()
{
	return Reflex::GetSingleton()->bReflexEnabled;
}

EXTERN_C bool GetLatencyReport(NV_LATENCY_RESULT_PARAMS* pGetLatencyParams)
{
	if (!Reflex::GetSingleton()->bReflexEnabled)
		return false;

	NvAPI_Status ret = NvAPI_D3D_GetLatency(g_Device, pGetLatencyParams);

	return (ret == NVAPI_OK);
}

EXTERN_C bool SetFPSLimit(float a_limit)
{
	if (!Reflex::GetSingleton()->bReflexEnabled)
		return false;
	if (a_limit < 0)
		return false;
	if (a_limit == 0) {
		Reflex::GetSingleton()->bFPSOverride = false;
	} else {
		Reflex::GetSingleton()->bFPSOverride = true;
		Reflex::GetSingleton()->fFPSOverrideLimit = a_limit;
	}
	return true;
}

// NVAPI

void Reflex::Initialize()
{
	if (InitializeNVAPI()) {
		logger::info("NVAPI Initialised");
		NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
		NvU32 gpuCount = 0;
		// Get all the Physical GPU Handles
		NvAPI_EnumPhysicalGPUs(nvGPUHandle, &gpuCount);
		if (gpuCount > 0) {
			logger::info("Found {} NVIDIA GPUs", gpuCount);
			gpuType = GPUType::NVIDIA;
			logger::info("Using NVAPI");
		} else {
			logger::error("Did not find a NVIDIA GPU");
		}
	} else {
		logger::error("Could not initialize NVAPI");
	}
}

bool Reflex::InitializeNVAPI()
{
	NvAPI_Status ret = NVAPI_OK;

	ret = NvAPI_Initialize();

	return ret == NVAPI_OK;
};

void Reflex::NVAPI_SetSleepMode()
{
	if (gpuType == GPUType::NVIDIA) {
		NvAPI_Status                status = NVAPI_OK;
		NV_SET_SLEEP_MODE_PARAMS_V1 params = { 0 };
		params.version = NV_SET_SLEEP_MODE_PARAMS_VER1;
		params.bLowLatencyMode = bLowLatencyMode;
		params.bLowLatencyBoost = bLowLatencyBoost;
		if (bFPSOverride)
			params.minimumIntervalUs = (NvU32)((1000.0f / fFPSOverrideLimit) * 1000.0f);
		else
			params.minimumIntervalUs = bUseFPSLimit ? (NvU32)((1000.0f / fFPSLimit) * 1000.0f) : 0;  // 0 means no requested framerate limit
		params.bUseMarkersToOptimize = bUseMarkersToOptimize;                                        // Only works with bLowLatencyBoost
		status = NvAPI_D3D_SetSleepMode(g_Device, &params);
		bReflexEnabled = (status == NVAPI_OK);
		if (status != lastStatus) {
			if (bReflexEnabled) {
				logger::info("Reflex enabled, returned status code {}", magic_enum::enum_name(status));
			} else {
				logger::info("Reflex not enabled, returned status code {}", magic_enum::enum_name(status));
			}
			lastStatus = status;
		}
	}
}

bool Reflex::NVAPI_SetLatencyMarker(NV_LATENCY_MARKER_TYPE marker)
{
	NvAPI_Status ret = NVAPI_INVALID_CONFIGURATION;

	if (bReflexEnabled) {
		NV_LATENCY_MARKER_PARAMS
		markerParams = {};
		markerParams.version = NV_LATENCY_MARKER_PARAMS_VER;
		markerParams.markerType = marker;
		markerParams.frameID = static_cast<NvU64>(ReadULong64Acquire(&frames_drawn));
		ret = NvAPI_D3D_SetLatencyMarker(g_Device, &markerParams);
	}

	if (marker == PRESENT_END)
		frames_drawn++;

	return (ret == NVAPI_OK);
}

// ENBSeries GUI

#define GetSettingFloat(a_section, a_setting) a_setting = (float)ini.GetDoubleValue(a_section, #a_setting, 1.0f);
#define SetSettingFloat(a_section, a_setting) ini.SetDoubleValue(a_section, #a_setting, a_setting);

#define GetSettingBool(a_section, a_setting) a_setting = ini.GetBoolValue(a_section, #a_setting, true);
#define SetSettingBool(a_section, a_setting) ini.SetBoolValue(a_section, #a_setting, a_setting);

void Reflex::LoadINI()
{
	std::lock_guard<std::shared_mutex> lk(fileLock);
	CSimpleIniA                        ini;
	ini.SetUnicode();
#ifndef FALLOUT4
	ini.LoadFile(L"Data\\SKSE\\Plugins\\NVIDIA_Reflex.ini");
#else
	ini.LoadFile(L"Data\\F4SE\\Plugins\\NVIDIA_Reflex.ini");
#endif
	GetSettingBool("NVIDIA Reflex", bLowLatencyMode);
	GetSettingBool("NVIDIA Reflex", bLowLatencyBoost);
	GetSettingBool("NVIDIA Reflex", bUseMarkersToOptimize);
	GetSettingBool("NVIDIA Reflex", bUseFPSLimit);
	GetSettingFloat("NVIDIA Reflex", fFPSLimit);
}

void Reflex::SaveINI()
{
	std::lock_guard<std::shared_mutex> lk(fileLock);
	CSimpleIniA                        ini;
	ini.SetUnicode();
	SetSettingBool("NVIDIA Reflex", bLowLatencyMode);
	SetSettingBool("NVIDIA Reflex", bLowLatencyBoost);
	SetSettingBool("NVIDIA Reflex", bUseMarkersToOptimize);
	SetSettingBool("NVIDIA Reflex", bUseFPSLimit);
	SetSettingFloat("NVIDIA Reflex", fFPSLimit);
#ifndef FALLOUT4
	ini.SaveFile(L"Data\\SKSE\\Plugins\\NVIDIA_Reflex.ini");
#else
	ini.SaveFile(L"Data\\F4SE\\Plugins\\NVIDIA_Reflex.ini");
#endif
}

extern ENB_API::ENBSDKALT1001* g_ENB;

void SetLowLatencyMode(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::GetSingleton()->bLowLatencyMode = *static_cast<const bool*>(value);
	Reflex::GetSingleton()->NVAPI_SetSleepMode();
}

void GetLowLatencyMode(void* value, [[maybe_unused]] void* clientData)
{
	*static_cast<bool*>(value) = Reflex::GetSingleton()->bLowLatencyMode;
}

void SetLowLatencyBoost(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::GetSingleton()->bLowLatencyBoost = *static_cast<const bool*>(value);
	Reflex::GetSingleton()->NVAPI_SetSleepMode();
}

void GetLowLatencyBoost(void* value, [[maybe_unused]] void* clientData)
{
	*static_cast<bool*>(value) = Reflex::GetSingleton()->bLowLatencyBoost;
}

void SetUseMarkersToOptimize(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::GetSingleton()->bUseMarkersToOptimize = *static_cast<const bool*>(value);
	Reflex::GetSingleton()->NVAPI_SetSleepMode();
}

void GetUseMarkersToOptimize(void* value, [[maybe_unused]] void* clientData)
{
	*static_cast<bool*>(value) = Reflex::GetSingleton()->bUseMarkersToOptimize;
}

void SetUseFPSLimit(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::GetSingleton()->bUseFPSLimit = *static_cast<const bool*>(value);
	Reflex::GetSingleton()->NVAPI_SetSleepMode();
}

void GetUseFPSLimit(void* value, [[maybe_unused]] void* clientData)
{
	*static_cast<bool*>(value) = Reflex::GetSingleton()->bUseFPSLimit;
}

void SetTargetFPS(const void* value, [[maybe_unused]] void* clientData)
{
	Reflex::GetSingleton()->fFPSLimit = *static_cast<const float*>(value);
	Reflex::GetSingleton()->NVAPI_SetSleepMode();
}

void GetTargetFPS(void* value, [[maybe_unused]] void* clientData)
{
	*static_cast<float*>(value) = Reflex::GetSingleton()->fFPSLimit;
}

void Reflex::RefreshUI()
{
#	ifndef FALLOUT4
	auto bar = g_ENB->TwGetBarByEnum(!REL::Module::IsVR() ? ENB_API::ENBWindowType::EditorBarEffects : ENB_API::ENBWindowType::EditorBarObjects);  // ENB misnames its own bar, whoops!
	#else
	auto bar = g_ENB->TwGetBarByEnum(ENB_API::ENBWindowType::EditorBarEffects);
	#endif
	g_ENB->TwAddVarRW(bar, "NVIDIA Reflex Enabled", ETwType::TW_TYPE_BOOLCPP, &bReflexEnabled, "group='MOD:NVIDIA Reflex' readonly=true");
	g_ENB->TwAddVarCB(bar, "Enable Low Latency Mode", ETwType::TW_TYPE_BOOLCPP, SetLowLatencyMode, GetLowLatencyMode, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Enable Low Latency Boost", ETwType::TW_TYPE_BOOLCPP, SetLowLatencyBoost, GetLowLatencyBoost, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Use Markers To Optimize", ETwType::TW_TYPE_BOOLCPP, SetUseMarkersToOptimize, GetUseMarkersToOptimize, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Enable FPS Limit", ETwType::TW_TYPE_BOOLCPP, SetUseFPSLimit, GetUseFPSLimit, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "FPS Limit", ETwType::TW_TYPE_FLOAT, SetTargetFPS, GetTargetFPS, this, "group='MOD:NVIDIA Reflex' min=30.00 max=1000.0 step=1.00");
}
