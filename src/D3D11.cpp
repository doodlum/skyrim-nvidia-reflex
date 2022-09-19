
#pragma comment(lib, "d3d11.lib")

#include <Detours.h>

#include "Reflex.h"

ID3D11Device*        g_Device;
ID3D11DeviceContext* g_DeviceContext;
IDXGISwapChain*      g_SwapChain;

#ifdef FALLOUT4
uintptr_t                                  g_ModuleBase;
HMODULE                                    g_DllD3D11;
#endif

decltype(&ID3D11DeviceContext::ClearState) ptrClearState;
decltype(&IDXGISwapChain::Present)         ptrPresent;
decltype(&D3D11CreateDeviceAndSwapChain)   ptrD3D11CreateDeviceAndSwapChain;

void WINAPI hk_ClearState(ID3D11DeviceContext* This)
{
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(RENDERSUBMIT_START);
	(This->*ptrClearState)();
}

HRESULT WINAPI hk_IDXGISwapChain_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
{
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(SIMULATION_END);
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(RENDERSUBMIT_END);
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(PRESENT_START);
	auto hr = (This->*ptrPresent)(SyncInterval, Flags);
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(PRESENT_END);
	if (Reflex::GetSingleton()->bReflexEnabled)
		NvAPI_D3D_Sleep(g_Device);
	Reflex::GetSingleton()->NVAPI_SetLatencyMarker(INPUT_SAMPLE);
	return hr;
}

HRESULT WINAPI hk_D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter*               pAdapter,
	D3D_DRIVER_TYPE             DriverType,
	HMODULE                     Software,
	UINT                        Flags,
	const D3D_FEATURE_LEVEL*    pFeatureLevels,
	UINT                        FeatureLevels,
	UINT                        SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	IDXGISwapChain**            ppSwapChain,
	ID3D11Device**              ppDevice,
	D3D_FEATURE_LEVEL*          pFeatureLevel,
	ID3D11DeviceContext**       ppImmediateContext)
{
	logger::info("Calling original D3D11CreateDeviceAndSwapChain");
	HRESULT hr = (*ptrD3D11CreateDeviceAndSwapChain)(pAdapter,
		DriverType,
		Software,
		Flags,
		pFeatureLevels,
		FeatureLevels,
		SDKVersion,
		pSwapChainDesc,
		ppSwapChain,
		ppDevice,
		pFeatureLevel,
		ppImmediateContext);

	logger::info("Storing render device information");
	g_Device = *ppDevice;
	g_DeviceContext = *ppImmediateContext;
	g_SwapChain = *ppSwapChain;

	return hr;
}

struct Hooks
{
#ifndef FALLOUT4
	struct BSGraphics_Renderer_Init_InitD3D
	{
		static void thunk()
		{
			logger::info("Calling original Init3D");
			func();
			logger::info("Accessing render device information");
			auto manager = RE::BSRenderManager::GetSingleton();
			g_Device = manager->GetRuntimeData().forwarder;
			g_DeviceContext = manager->GetRuntimeData().context;
			g_SwapChain = manager->GetRuntimeData().swapChain;
			logger::info("Detouring virtual function tables");
			*(uintptr_t*)&ptrPresent = Detours::X64::DetourClassVTable(*(uintptr_t*)g_SwapChain, &hk_IDXGISwapChain_Present, 8);
			*(uintptr_t*)&ptrClearState = Detours::X64::DetourClassVTable(*(uintptr_t*)g_DeviceContext, &hk_ClearState, 110);
			logger::info("Setting sleep mode for the first time");
			Reflex::GetSingleton()->NVAPI_SetSleepMode();
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
#else
	struct BSGraphics_Renderer_Init_InitD3D
	{
		static void thunk(std::uintptr_t a_this, std::uintptr_t a_RendererInitOSData)
		{
			logger::info("Calling original Init3D");
			func(a_this, a_RendererInitOSData);
			logger::info("Detouring virtual function tables");
			*(uintptr_t*)&ptrPresent = Detours::X64::DetourClassVTable(*(uintptr_t*)g_SwapChain, &hk_IDXGISwapChain_Present, 8);
			*(uintptr_t*)&ptrClearState = Detours::X64::DetourClassVTable(*(uintptr_t*)g_DeviceContext, &hk_ClearState, 110);
			logger::info("Initializing NVAPI");
			Reflex::GetSingleton()->Initialize();
			logger::info("Setting sleep mode for the first time");
			Reflex::GetSingleton()->NVAPI_SetSleepMode();
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
#endif
	#define PatchIAT(detour, module, procname) Detours::IATHook(g_ModuleBase, (module), (procname), (uintptr_t)(detour));

	static void Install()
	{
#ifdef FALLOUT4
		g_ModuleBase = (uintptr_t)GetModuleHandle(nullptr);
		g_DllD3D11 = GetModuleHandleA("d3d11.dll");
		if (g_DllD3D11) {
			*(FARPROC*)&ptrD3D11CreateDeviceAndSwapChain = GetProcAddress(g_DllD3D11, "D3D11CreateDeviceAndSwapChain");
			PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
			logger::info("Patched d3d11.dll address table");
		} else {
			logger::critical("Failed to patch d3d11.dll address table");
		}
		stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(REL::ID(564405).address() + 0x12B);
#else
		stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));
#endif
		logger::info("Installed render startup hook");
	}
};

void PatchD3D11()
{
	Hooks::Install();
}
