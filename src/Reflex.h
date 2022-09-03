#pragma once

#include <d3d11.h>
#include <dxgi.h>

#pragma warning(push)
#pragma warning(disable: 4828)
#include <NVAPI/nvapi.h>
#pragma warning(pop)

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Reflex
{
public:
	static Reflex* GetSingleton()
	{
		static Reflex handler;
		return &handler;
	}

	static void InstallHooks()
	{
		Hooks::Install();
	}

	void Initialize();

	void NVAPI_SetSleepMode();
	bool NVAPI_SetLatencyMarker(NV_LATENCY_MARKER_TYPE marker);

	enum class GPUType
	{
		NVIDIA,
		AMD,
		INTEL,
		UNKNOWN
	};

	NvAPI_Status lastStatus = NVAPI_INVALID_CONFIGURATION;

	bool  bReflexEnabled = false;
	bool  bFPSOverride = false;
	float fFPSOverrideLimit = 60;

	bool  bLowLatencyMode = true;
	bool  bLowLatencyBoost = true;
	bool  bUseMarkersToOptimize = true;
	bool  bSleepOnSimulationStart = false;
	bool  bUseFPSLimit = false;
	float fFPSLimit = 60;

	json JSONSettings;

	void LoadJSON();
	void SaveJSON();

	void RefreshUI();

protected:
	struct Hooks
	{
		struct MainLoop_Update
		{
			static char thunk(RE::Main* a_main)
			{
				Reflex::GetSingleton()->NVAPI_SetLatencyMarker(SIMULATION_START);
				Reflex::GetSingleton()->NVAPI_SetLatencyMarker(INPUT_SAMPLE);
				return func(a_main);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<MainLoop_Update>(REL::RelocationID(35551, 36550).address() + REL::Relocate(0x11F, 0x11F));
		}
	};

private:
	Reflex() { LoadJSON(); };

	Reflex(const Reflex&) = delete;
	Reflex(Reflex&&) = delete;

	~Reflex() = default;

	Reflex& operator=(const Reflex&) = delete;
	Reflex& operator=(Reflex&&) = delete;

	DWORD64 frames_drawn = 0;

	// NVAPI

	typedef int* (*NVAPI_QueryInterface_t)(unsigned int offset);
	typedef int (*NVAPI_Initialize_t)();
	typedef int (*NVAPI_EnumPhysicalGPUs_t)(int** handles, int* count);

	NVAPI_QueryInterface_t   NVAPI_QueryInterface = NULL;
	NVAPI_Initialize_t       NVAPI_Initialize = NULL;
	NVAPI_EnumPhysicalGPUs_t NVAPI_EnumPhysicalGPUs = NULL;

	int  NVAPI_gpuCount = 0;
	int* NVAPI_gpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { NULL };

	bool InitializeNVAPI();

	// Shared

	GPUType gpuType = GPUType::UNKNOWN;
};
