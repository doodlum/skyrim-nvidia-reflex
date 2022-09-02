
#pragma comment(lib, "d3d11.lib")

#include <Detours.h>

#include "Reflex.h"

ID3D11Device*        g_Device;
ID3D11DeviceContext* g_DeviceContext;
IDXGISwapChain*      g_SwapChain;

decltype(&ID3D11DeviceContext::ClearState) ptrClearState;
decltype(&IDXGISwapChain::Present)         ptrPresent;
decltype(&D3D11CreateDeviceAndSwapChain)   ptrD3D11CreateDeviceAndSwapChain;

uintptr_t g_ModuleBase;
HMODULE   g_DllDXGI;
HMODULE   g_DllD3D11;

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

	g_Device = *ppDevice;
	g_DeviceContext = *ppImmediateContext;
	g_SwapChain = *ppSwapChain;

	*(uintptr_t*)&ptrPresent = Detours::X64::DetourClassVTable(*(uintptr_t*)*ppSwapChain, &hk_IDXGISwapChain_Present, 8);
	*(uintptr_t*)&ptrClearState = Detours::X64::DetourClassVTable(*(uintptr_t*)*ppImmediateContext, &hk_ClearState, 110);

	Reflex::GetSingleton()->NVAPI_SetSleepMode();

	return hr;
}

#define PatchIAT(detour, module, procname) Detours::IATHook(g_ModuleBase, (module), (procname), (uintptr_t)(detour));

void PatchD3D11()
{
	g_ModuleBase = (uintptr_t)GetModuleHandle(nullptr);

	if (!g_DllD3D11)
		g_DllD3D11 = GetModuleHandleA("d3d11.dll");

	*(FARPROC*)&ptrD3D11CreateDeviceAndSwapChain = GetProcAddress(g_DllD3D11, "D3D11CreateDeviceAndSwapChain");

	PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
}
