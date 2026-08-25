#include "Hooks.h"

#include "Minimap.h"

#include "utils/Logger.h"

void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate)
{
	logger::debug("HUDMenu::Accept hook fired");

	hooks::HUDMenu::Accept(a_hudMenu, a_gameDelegate);

	a_gameDelegate->Process("SetLocalMapExtents",
		[](const RE::FxDelegateArgs& a_delegateArgs) -> void
		{
			logger::debug("SetLocalMapExtents callback: left {}, top {}, right {}, bottom {}",
						  a_delegateArgs[0].GetNumber(), a_delegateArgs[1].GetNumber(),
						  a_delegateArgs[2].GetNumber(), a_delegateArgs[3].GetNumber());
			DEM::Minimap::GetSingleton()->SetLocalMapExtents(a_delegateArgs);
		});
}

void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime)
{
	// AdvanceMovie runs every frame the HUD menu is open, so only note that the hook is
	// live once - logging it on every call would flood the log without adding information.
	static bool loggedFirstAdvance = false;
	if (!loggedFirstAdvance)
	{
		loggedFirstAdvance = true;
		logger::debug("HUDMenu::AdvanceMovie hook fired for the first time; hook is active (runs every frame, not logged again)");
	}

	hooks::HUDMenu::AdvanceMovie(a_hudMenu, a_interval, a_currentTime);

	a_hudMenu->menuFlags.set(RE::UI_MENU_FLAGS::kRendersOffscreenTargets);
	DEM::Minimap::GetSingleton()->Advance();
}

void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu)
{
	// PreDisplay runs every frame the HUD menu is open too - same reasoning as above for
	// only logging the hook's first firing rather than every frame.
	static bool loggedFirstPreDisplay = false;
	if (!loggedFirstPreDisplay)
	{
		loggedFirstPreDisplay = true;
		logger::debug("HUDMenu::PreDisplay hook fired for the first time; hook is active (runs every frame, not logged again)");
	}

	auto miniMap = DEM::Minimap::GetSingleton();

	// Visibility itself changes rarely (user toggle, menu open/close), so logging on
	// transitions rather than every frame stays useful without spamming.
	static bool wasVisible = false;
	bool isVisible = miniMap->IsVisible();
	if (isVisible != wasVisible)
	{
		logger::debug("Minimap visibility changed: {} -> {}", wasVisible, isVisible);
		wasVisible = isVisible;
	}

	if (miniMap->IsVisible())
	{
		DEM::Minimap::GetSingleton()->PreRender();
	}

	hooks::HUDMenu::PreDisplay(a_hudMenu);
}
