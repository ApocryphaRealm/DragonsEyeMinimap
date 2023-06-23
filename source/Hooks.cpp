#include "Hooks.h"

#include "utils/Logger.h"

#include "RE/H/HUDMenu.h"

#include "RE/RTTI.h"

#include "Minimap.h"


void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate)
{
	hooks::HUDMenu::Accept(a_hudMenu, a_gameDelegate);

	a_gameDelegate->Process("SetLocalMapExtents",
		[](const RE::FxDelegateArgs& a_delegateArgs) -> void
		{
			DEM::Minimap::GetSingleton()->SetLocalMapExtents(a_delegateArgs);
		});

	a_gameDelegate->Process("UpdateOnEnterFrame",
		[](const RE::FxDelegateArgs& a_delegateArgs) -> void
		{
			DEM::Minimap::GetSingleton()->UpdateOnEnterFrame(a_delegateArgs);
		});
}

void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime)
{
	hooks::HUDMenu::AdvanceMovie(a_hudMenu, a_interval, a_currentTime);

	DEM::Minimap* minimap = DEM::Minimap::GetSingleton();
	if (minimap)
	{
		a_hudMenu->menuFlags.set(RE::UI_MENU_FLAGS::kRendersOffscreenTargets);
		minimap->Advance();
	}
}

void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu)
{
	if (!RE::UI::GetSingleton()->IsMenuOpen(RE::MapMenu::MENU_NAME))
	{
		DEM::Minimap* minimap = DEM::Minimap::GetSingleton();
		if (minimap)
		{
			minimap->PreRender();
		}
	}

	hooks::HUDMenu::PreDisplay(a_hudMenu);
}