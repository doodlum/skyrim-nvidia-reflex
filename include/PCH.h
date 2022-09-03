#pragma once

#pragma warning(push)
#pragma warning(disable: 5105)
#pragma warning(disable: 4189)

#if defined(FALLOUT4)
#	include "F4SE/F4SE.h"
#	include "RE/Fallout.h"
#	define SKSE F4SE
#	define SKSEAPI F4SEAPI
#	define SKSEPlugin_Load F4SEPlugin_Load
#	define SKSEPlugin_Query F4SEPlugin_Query
#	define RUNTIME RUNTIME_1_10_163
#else
#	include "RE/Skyrim.h"
#	include "SKSE/SKSE.h"
#	if defined(SKYRIMAE)
#		define RUNTIME 0
#	elif defined(SKYRIMVR)
#		define RUNTIME SKSE::RUNTIME_VR_1_4_15_1
#	else
#		define RUNTIME SKSE::RUNTIME_1_5_97
#	endif
#endif

//#undef NDEBUG

#define MAGIC_ENUM_RANGE_MAX 256
#include <magic_enum.hpp>

#include <Windows.h>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif

#pragma warning(pop)

using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(14);

		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class T>
	void write_thunk_call6(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(14);

		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<6>(a_src, T::thunk);
	}

	template <class F, std::size_t idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <std::uintptr_t address, std::size_t idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ address };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <std::size_t idx, class T>
	void write_vfunc(std::uintptr_t id)
	{
		REL::Relocation<std::uintptr_t> vtbl{ id };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}
}

namespace logger = SKSE::log;

namespace util
{
	using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"
