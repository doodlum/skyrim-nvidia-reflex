#include "Reflex.h"
#include <ENB/ENBSeriesAPI.h>

extern ID3D11DeviceContext* g_DeviceContext;
extern ID3D11Device*        g_Device;
extern IDXGISwapChain*      g_SwapChain;

extern uintptr_t g_ModuleBase;
extern HMODULE   g_DllDXGI;
extern HMODULE   g_DllD3D11;

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
		if (NVAPI_gpuCount > 0) {
			logger::info("Found {} NVIDIA GPUs", NVAPI_gpuCount);
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
	HMODULE hNVAPI_DLL = LoadLibraryA("nvapi64.dll");

	if (!hNVAPI_DLL)
		return false;

	// nvapi_QueryInterface is a function used to retrieve other internal functions in nvapi.dll
	NVAPI_QueryInterface = (NVAPI_QueryInterface_t)GetProcAddress(hNVAPI_DLL, "nvapi_QueryInterface");

	// Some useful internal functions that aren't exported by nvapi.dll
	NVAPI_Initialize = (NVAPI_Initialize_t)(*NVAPI_QueryInterface)(0x0150E828);
	NVAPI_EnumPhysicalGPUs = (NVAPI_EnumPhysicalGPUs_t)(*NVAPI_QueryInterface)(0xE5AC921F);

	if (NVAPI_Initialize != nullptr &&
		NVAPI_EnumPhysicalGPUs != nullptr) {
		(*NVAPI_Initialize)();
		(*NVAPI_EnumPhysicalGPUs)(NVAPI_gpuHandles, &NVAPI_gpuCount);
		return true;
	}

	return false;
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
		if (marker == SIMULATION_START && bSleepOnSimulationStart)
			NvAPI_D3D_Sleep(g_Device);
		if (marker == PRESENT_END && !bSleepOnSimulationStart)
			NvAPI_D3D_Sleep(g_Device);
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

void Reflex::LoadJSON()
{
	std::ifstream i(L"Data\\SKSE\\Plugins\\NVIDIA_Reflex.json");
	i >> JSONSettings;

	bLowLatencyMode = JSONSettings["bLowLatencyMode"];
	bLowLatencyBoost = JSONSettings["bLowLatencyBoost"];
	bUseMarkersToOptimize = JSONSettings["bUseMarkersToOptimize"];

	bSleepOnSimulationStart = JSONSettings["bSleepOnSimulationStart"];

	bUseFPSLimit = JSONSettings["bUseFPSLimit"];
	fFPSLimit = JSONSettings["fFPSLimit"];
}

void Reflex::SaveJSON()
{
	std::ofstream o(L"Data\\SKSE\\Plugins\\NVIDIA_Reflex.json");

	JSONSettings["bLowLatencyMode"] = bLowLatencyMode;
	JSONSettings["bLowLatencyBoost"] = bLowLatencyBoost;
	JSONSettings["bUseMarkersToOptimize"] = bUseMarkersToOptimize;

	JSONSettings["bSleepOnSimulationStart"] = bSleepOnSimulationStart;

	JSONSettings["bUseFPSLimit"] = bUseFPSLimit;
	JSONSettings["fFPSLimit"] = fFPSLimit;

	o << JSONSettings.dump(1);
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
	auto bar = g_ENB->TwGetBarByEnum(!REL::Module::IsVR() ? ENB_API::ENBWindowType::EditorBarEffects : ENB_API::ENBWindowType::EditorBarObjects);  // ENB misnames its own bar, whoops!
	g_ENB->TwAddVarRW(bar, "NVIDIA Reflex Enabled", ETwType::TW_TYPE_BOOLCPP, &bReflexEnabled, "group='MOD:NVIDIA Reflex' readonly=true");
	g_ENB->TwAddVarCB(bar, "Enable Low Latency Mode", ETwType::TW_TYPE_BOOLCPP, SetLowLatencyMode, GetLowLatencyMode, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Enable Low Latency Boost", ETwType::TW_TYPE_BOOLCPP, SetLowLatencyBoost, GetLowLatencyBoost, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Use Markers To Optimize", ETwType::TW_TYPE_BOOLCPP, SetUseMarkersToOptimize, GetUseMarkersToOptimize, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarRW(bar, "Sleep On Simulation Start", ETwType::TW_TYPE_BOOLCPP, &bSleepOnSimulationStart, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "Enable FPS Limit", ETwType::TW_TYPE_BOOLCPP, SetUseFPSLimit, GetUseFPSLimit, this, "group='MOD:NVIDIA Reflex'");
	g_ENB->TwAddVarCB(bar, "FPS Limit", ETwType::TW_TYPE_FLOAT, SetTargetFPS, GetTargetFPS, this, "group='MOD:NVIDIA Reflex' min=30.00 max=1000.0 step=1.00");
}
