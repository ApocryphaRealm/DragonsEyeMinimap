#pragma once

#include "utils/Logger.h"
#include "utils/Trampoline.h"

void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate);
void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime);
void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu);

namespace hooks
{
	class HUDMenu
	{
		static constexpr REL::RelocationID ProcessMessageId{ 50718, 51612 };

	public:
		// Nothing in this file resolves an address at static scope. A static REL::Relocation built from an Address
		// Library ID is constructed by the C runtime while the DLL loads - before SKSEPluginLoad - and constructing it
		// opens the Address Library, so a missing or wrong one showed CommonLibSSE-NG's bare box before the Address
		// Library guard could run (gate row no-address-resolved-at-static-init; the Infinity UI 2.1.3 fix). The vtable
		// and the addresses below are resolved when Install() asks for them.
		static REL::Relocation<std::uintptr_t> VTable() { return REL::Relocation<std::uintptr_t>{ RE::VTABLE_HUDMenu[0] }; }

		static inline REL::Relocation<void (RE::HUDMenu::*)(RE::FxDelegateHandler::CallbackProcessor*)> Accept;
		static inline REL::Relocation<void (RE::HUDMenu::*)(float, std::uint32_t)> AdvanceMovie;
		static inline REL::Relocation<void (RE::HUDMenu::*)()> PreDisplay;
	};

	class LocalMapMenu
	{
		static constexpr REL::RelocationID CtorId{ 52076, 52964 };
		static constexpr REL::RelocationID RefreshMarkersId{ 52090, 52980 };

	public:
		class LocalMapCullingProcess
		{
			static constexpr REL::RelocationID RenderOffScreenId{ 16094, 16335 };

		public:
			static std::uintptr_t RenderOffScreenAddress() { return RenderOffScreenId.address(); }
		};

		static std::uintptr_t CtorAddress() { return CtorId.address(); }
		static std::uintptr_t RefreshMarkersAddress() { return RefreshMarkersId.address(); }
	};

	class NiCamera
	{
	public:
		static inline REL::Relocation<bool (RE::NiCamera::*)(const RE::NiPoint3&, float&, float&, float&, float)> WorldPtToScreenPt3;
	};

	static inline void Install()
	{
		// One line per hook, including the resolved vtable address, so a hook that silently
		// fails to take effect on a game version this plugin wasn't built against - no crash,
		// no error, just nothing working - has something to compare against a known-good log
		// instead of being a total mystery from a bug report alone.
		REL::Relocation<std::uintptr_t> vTable = HUDMenu::VTable();
		HUDMenu::Accept = vTable.write_vfunc(1, AcceptHUDMenu);
		logger::debug("Hook installed: HUDMenu::Accept (vfunc 1) at {:#x}", vTable.address());

		HUDMenu::AdvanceMovie = vTable.write_vfunc(5, AdvanceMovieHUDMenu);
		logger::debug("Hook installed: HUDMenu::AdvanceMovie (vfunc 5) at {:#x}", vTable.address());

		HUDMenu::PreDisplay = vTable.write_vfunc(7, PreDisplayHUDMenu);
		logger::debug("Hook installed: HUDMenu::PreDisplay (vfunc 7) at {:#x}", vTable.address());

		logger::debug("LocalMapMenu::Ctor resolved to {:#x}", LocalMapMenu::CtorAddress());
		logger::debug("LocalMapMenu::RefreshMarkers resolved to {:#x}", LocalMapMenu::RefreshMarkersAddress());
		logger::debug("LocalMapMenu::LocalMapCullingProcess::RenderOffScreen resolved to {:#x}",
			LocalMapMenu::LocalMapCullingProcess::RenderOffScreenAddress());
	}
}
