
#pragma comment(lib, "d3d11.lib")

#include <Detours.h>

#include "Reflex.h"

ID3D11Device*        g_Device;
ID3D11DeviceContext* g_DeviceContext;
IDXGISwapChain*      g_SwapChain;

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
	return hr;
}

struct Hooks
{


	struct CalledDuringRenderStartup
	{
		static void thunk()
		{
			logger::info("Calling original function");
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

	static void Install()
	{
		stl::write_thunk_call<CalledDuringRenderStartup>(REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));
		logger::info("Installed render startup hook");
	}
};

void PatchD3D11()
{
	Hooks::Install();
}
