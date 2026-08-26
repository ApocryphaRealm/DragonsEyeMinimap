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

			// This can fire before the Infinity UI patch pipeline has created the minimap
			// singleton (HUDMenu::Accept runs on the engine's own schedule, not ours).
			if (auto* miniMap = DEM::Minimap::GetSingleton())
			{
				miniMap->SetLocalMapExtents(a_delegateArgs);
			}
			else
			{
				logger::warn("SetLocalMapExtents callback fired before the minimap singleton exists; ignoring");
			}
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

	// This runs every frame the HUD menu is open, which can start before the Infinity UI
	// patch pipeline has created the minimap singleton.
	if (auto* miniMap = DEM::Minimap::GetSingleton())
	{
		miniMap->Advance();
	}
	else
	{
		static bool warnedMissingSingleton = false;
		if (!warnedMissingSingleton)
		{
			logger::warn("AdvanceMovie hook fired before the minimap singleton exists; skipping Advance() until it is ready");
			warnedMissingSingleton = true;
		}
	}
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

	auto* miniMap = DEM::Minimap::GetSingleton();

	// This runs every frame the HUD menu is open, which can start before the Infinity UI
	// patch pipeline has created the minimap singleton.
	if (!miniMap)
	{
		static bool warnedMissingSingleton = false;
		if (!warnedMissingSingleton)
		{
			logger::warn("PreDisplay hook fired before the minimap singleton exists; skipping until it is ready");
			warnedMissingSingleton = true;
		}
	}
	else
	{
		// Visibility itself changes rarely (user toggle, menu open/close), so logging on
		// transitions rather than every frame stays useful without spamming.
		// One-shot: does the clip actually OWN the HUD mode flags?
		//
		// ShowElements hides an element by testing hasOwnProperty(mode) on it. The flags are
		// declared on the minimap clip's timeline as bare `var All;` and are only ASSIGNED inside
		// the Minimap() constructor - which nothing in this plugin ever invokes. If a declared but
		// unassigned timeline var does not create an own property, the test fails every time
		// ShowElements runs, and the element is hidden while the mode stack innocently reads
		// "All". That matches every observation so far, including why disabling every other HUD
		// mod changed nothing.
		{
			static bool reportedFlags = false;

			if (!reportedFlags)
			{
				reportedFlags = true;
				miniMap->ReportModeFlagOwnership();
			}
		}

		static bool wasVisible = false;
		bool isVisible = miniMap->IsVisible();
		if (isVisible != wasVisible)
		{
			// Report the mode stack alongside the change. When something hides this element, the
			// mode on top of the stack is the flag the clip does not declare - this turns a guess
			// among seventeen modes into a name.
			logger::info("Minimap visibility changed: {} -> {} | HUD modes: {}",
						 wasVisible, isVisible, miniMap->DescribeHudModes());
			wasVisible = isVisible;
		}

		// Restore visibility in the SAME frame it was lost, before anything is drawn.
		//
		// Something outside this mod clears the clip's _visible about twice a second during play.
		// It is not the ActionScript (proven by reverting the SWF to its last known-good copy),
		// not this mod's own code (nothing here writes _visible per frame), not ImmersiveHUD,
		// moreHUD or TrueHUD (all disabled, 114 transitions against 116 with them on), not the
		// HUD mode system (the stack reads "All" in 111 of 114 samples), and not the map's
		// re-initialisation cycle (0 of 57 hides fell within 150ms of one).
		//
		// The ActionScript sync restores it, but only on the NEXT frame, so one blank frame
		// reaches the screen every time - which is the flicker. This hook runs every frame BEFORE
		// rendering, so correcting it here means the blank frame is never drawn at all.
		//
		// Gated on the HUD mode, which is what makes this safe where 1.3.2's blind re-assert was
		// not: in WorldMapMode or any menu mode this does nothing, so the world map still hides
		// the minimap normally.
		if (!isVisible && miniMap->IsShown() && miniMap->HudModeAllowsMinimap())
		{
			miniMap->SetDisplayObjectVisible(true);
			isVisible = miniMap->IsVisible();
		}

		if (isVisible)
		{
			miniMap->PreRender();
		}
	}

	hooks::HUDMenu::PreDisplay(a_hudMenu);
}
