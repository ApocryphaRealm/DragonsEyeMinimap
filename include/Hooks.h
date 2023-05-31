#pragma once

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
		static inline REL::Relocation<std::uintptr_t> vTable{ RE::VTABLE_HUDMenu[0] };

		static inline REL::Relocation<void (RE::HUDMenu::*)(RE::FxDelegateHandler::CallbackProcessor*)> Accept;
		static inline REL::Relocation<void (RE::HUDMenu::*)(float, std::uint32_t)> AdvanceMovie;
		static inline REL::Relocation<void (RE::HUDMenu::*)()> PreDisplay;
	};

	class LocalMapMenu
	{
		static constexpr REL::RelocationID CtorId{ 52076, 0 };
		static constexpr REL::RelocationID RefreshMarkersId{ 52090, 0 };

	public:
		class LocalMapCullingProcess;

		static inline REL::Relocation<RE::LocalMapMenu* (RE::LocalMapMenu::*)()> Ctor{ CtorId };
		static inline REL::Relocation<void (RE::LocalMapMenu::*)()> RefreshMarkers{ RefreshMarkersId };
	};

	class NiCamera
	{
	public:
		static inline REL::Relocation<bool (RE::NiCamera::*)(const RE::NiPoint3&, float&, float&, float&, float)> WorldPtToScreenPt3;
	};


	class LocalMapMenu::LocalMapCullingProcess
	{
		static constexpr REL::RelocationID RenderOffScreenId{ 16094, 0 };

	public:
		static inline REL::Relocation<void (RE::LocalMapMenu::LocalMapCullingProcess::*)()> RenderOffScreen{ RenderOffScreenId };
	};

	static inline void Install()
	{
		HUDMenu::Accept = HUDMenu::vTable.write_vfunc(1, AcceptHUDMenu);
		HUDMenu::AdvanceMovie = HUDMenu::vTable.write_vfunc(5, AdvanceMovieHUDMenu);
		HUDMenu::PreDisplay = HUDMenu::vTable.write_vfunc(7, PreDisplayHUDMenu);
	}
}
