#include "Hooks.h"

#include "Minimap.h"

void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate)
{
	hooks::HUDMenu::Accept(a_hudMenu, a_gameDelegate);

	a_gameDelegate->Process("SetLocalMapExtents",
		[](const RE::FxDelegateArgs& a_delegateArgs) -> void
		{
			DEM::Minimap::GetSingleton()->SetLocalMapExtents(a_delegateArgs);
		});
}

void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime)
{
	hooks::HUDMenu::AdvanceMovie(a_hudMenu, a_interval, a_currentTime);

	a_hudMenu->menuFlags.set(RE::UI_MENU_FLAGS::kRendersOffscreenTargets);
	DEM::Minimap::GetSingleton()->Advance();
}

void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu)
{
	auto miniMap = DEM::Minimap::GetSingleton();

	if (miniMap->IsVisible())
	{
		DEM::Minimap::GetSingleton()->PreRender();
	}

	hooks::HUDMenu::PreDisplay(a_hudMenu);
}

void RefreshPlatformHUDMenu(RE::HUDMenu* a_hudMenu)
{
	hooks::HUDMenu::RefreshPlatform(a_hudMenu);

	DEM::Minimap::GetSingleton()->RefreshPlatform();
}

bool CanProcessMenuOpenHandler(RE::MenuOpenHandler* a_menuOpenHandler, RE::InputEvent* a_event)
{
	if (a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
	{
		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			auto userEvents = RE::UserEvents::GetSingleton();

			const RE::BSFixedString& buttonUserEvent = buttonEvent->QUserEvent();

			// Defer wait button
			if (buttonUserEvent == userEvents->wait)
			{
				if (buttonEvent->IsDown())
				{
					return false;
				}
				else if (buttonEvent->IsUp() && !DEM::Minimap::GetSingleton()->IsShown())
				{
					buttonEvent->GetRuntimeData().value = 1.0F;
					buttonEvent->GetRuntimeData().heldDownSecs = 0.0F;
				}
			}
		}
	}

	return hooks::MenuOpenHandler::CanProcess(a_menuOpenHandler, a_event);
}
