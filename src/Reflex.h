#pragma once

#include <shared_mutex>

#include <d3d11.h>
#include <dxgi.h>

#pragma warning(push)
#pragma warning(disable: 4828)
#include <NVAPI/nvapi.h>
#pragma warning(pop)

extern ID3D11DeviceContext* g_DeviceContext;
extern ID3D11Device*        g_Device;
extern IDXGISwapChain*      g_SwapChain;

extern uintptr_t g_ModuleBase;
extern HMODULE   g_DllDXGI;
extern HMODULE   g_DllD3D11;

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

	std::shared_mutex fileLock;

	// Shared

	void Initialize();

	// NVAPI

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
	bool  bUseFPSLimit = false;
	float fFPSLimit = 60;

	void LoadINI();
	void SaveINI();

	void RefreshUI();

protected:
	struct Hooks
	{
		struct Main_Update_Start
		{
			static void thunk(INT64 a_unk)
			{
				Reflex::GetSingleton()->NVAPI_SetLatencyMarker(SIMULATION_START);
#ifndef FALLOUT4
				Reflex::GetSingleton()->NVAPI_SetLatencyMarker(INPUT_SAMPLE);
#endif
				func(a_unk);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	
#ifndef FALLOUT4
		struct Main_Update_Timer
		{
			static void thunk(RE::Main* a_main)
			{
				if (GetSingleton()->bReflexEnabled)
					NvAPI_D3D_Sleep(g_Device);
				func(a_main);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
#endif

		static void Install()
		{
			#ifndef FALLOUT4
			stl::write_thunk_call<Main_Update_Start>(REL::RelocationID(35565, 36564).address() + REL::Relocate(0x1E, 0x3E, 0x33));
			stl::write_thunk_call<Main_Update_Timer>(REL::RelocationID(35565, 36564).address() + REL::Relocate(0x5E3, 0xAA3, 0x689));
			#else
			stl::write_thunk_call<Main_Update_Start>(REL::ID(1125396).address() + 0xBB);
			#endif
		}
	};

private:
	Reflex() { LoadINI(); };

	Reflex(const Reflex&) = delete;
	Reflex(Reflex&&) = delete;

	~Reflex() = default;

	Reflex& operator=(const Reflex&) = delete;
	Reflex& operator=(Reflex&&) = delete;

	DWORD64 frames_drawn = 0;

	// Shared

	GPUType gpuType = GPUType::UNKNOWN;

	// NVAPI

	bool InitializeNVAPI();
};
