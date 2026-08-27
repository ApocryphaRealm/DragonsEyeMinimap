#include "Minimap.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"

namespace RE
{
	bool UI__IsInMenuMode()
	{
		using func_t = decltype(&UI__IsInMenuMode);
		REL::Relocation<func_t> func{ RELOCATION_ID(56476, 56833) };
		return func();
	}
}

namespace DEM
{
	bool Minimap::InputHandler::CanProcess(RE::InputEvent* a_event)
	{
		// Menu mode only. Deliberately NOT gated on IsVisible() any more.
		//
		// It used to deregister the handler whenever the minimap was not visible, which made the
		// hide key a one-way switch: it could hide the map, and then - because the handler was
		// gone the moment it became invisible - it could never show it again. the author hit exactly
		// that: "whenever I set the keybind to hide, it won't trigger it to show like toggling it
		// would, but it will turn off the map if I use the keybind."
		//
		// A toggle has to work in both states, so the only thing that should suppress it is the
		// player being in a menu, where the key belongs to the menu.
		if (RE::UI__IsInMenuMode())
		{
			if (registered)
			{
				logger::debug("Minimap no longer eligible to process input (menu open); deregistering input handler");
				menuControls->RemoveHandler(this);
			}

			return false;
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessButton(RE::ButtonEvent* a_event)
	{
		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			// Only the keyboard hide/zoom keys are handled here now; there is no gamepad or
			// mouse behaviour left to route to.
			if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
			{
				return ProcessKeyboardOrMouseButton(buttonEvent);
			}
		}

		return false;
	}

	bool Minimap::InputHandler::ProcessKeyboardOrMouseButton(RE::ButtonEvent* a_buttonEvent)
	{
		// Two dedicated keys, pressed rather than held, each doing one thing the moment they
		// go down. The device check that used to matter here (mouse IDCodes are 0-7 and
		// overlap the low DirectInput scan codes) is now redundant with ProcessButton only
		// ever routing keyboard events here, but is harmless to keep implicit.
		if (a_buttonEvent->IsDown())
		{
			if (settings::controls::hideKeyCode > 0 &&
				a_buttonEvent->GetIDCode() == static_cast<std::uint32_t>(settings::controls::hideKeyCode))
			{
				logger::debug("Hide key pressed (code {}) - minimap now {}", a_buttonEvent->GetIDCode(), miniMap->IsShown() ? "hidden" : "shown");

				// Recorded separately from Show()/Hide() below: the settings page can call those
				// too, so "the keybind fired" and "the minimap changed" are different facts and a
				// dead keybind is only visible when they are reported apart.

				// Runtime toggle: change what is drawn, leave bShowOnGameStart alone.
				miniMap->IsShown() ? miniMap->Hide(false) : miniMap->Show(false);

				return true;
			}

			if (settings::controls::zoomToggleKeyCode > 0 &&
				a_buttonEvent->GetIDCode() == static_cast<std::uint32_t>(settings::controls::zoomToggleKeyCode))
			{
				logger::debug("Zoom toggle key pressed (code {})", a_buttonEvent->GetIDCode());


				miniMap->ToggleZoomPreset();

				return true;
			}
		}

		return false;
	}

	std::string Minimap::DescribeHudModes() const
	{
		// The minimap clip is a child of HUDMovieBaseInstance, so its _parent is the object that
		// owns the mode stack.
		RE::GFxValue parent = displayObj.GetMember("_parent");

		if (!parent.IsDisplayObject())
		{
			return "<no parent>";
		}

		RE::GFxValue modes;

		if (!parent.GetMember("HUDModes", &modes) || !modes.IsArray())
		{
			return "<no HUDModes>";
		}

		const std::uint32_t count = modes.GetArraySize();

		if (count == 0)
		{
			return "<empty stack, treated as All>";
		}

		std::string out;

		for (std::uint32_t i = 0; i < count; ++i)
		{
			RE::GFxValue entry;

			if (modes.GetElement(i, &entry) && entry.IsString())
			{
				if (!out.empty())
				{
					out += " -> ";
				}

				out += entry.GetString();
			}
		}

		return out.empty() ? "<unreadable entries>" : out;
	}

	void Minimap::ReportModeFlagOwnership() const
	{
		// Every mode vanilla HUDMenu can push, so the log shows the whole picture rather than
		// only the ones this clip was authored with.
		constexpr const char* kModes[] = {
			"All", "Favor", "InventoryMode", "TweenMode", "BookMode", "DialogueMode",
			"BarterMode", "WorldMapMode", "MovementDisabled", "StealthMode", "Swimming",
			"HorseMode", "WarHorseMode", "CartMode", "SleepWaitMode", "JournalMode",
			"VATSPlayback"
		};

		std::string owned;
		std::string missing;

		for (const char* mode : kModes)
		{
			std::string& target = displayObj.HasMember(mode) ? owned : missing;

			if (!target.empty())
			{
				target += ", ";
			}

			target += mode;
		}

		logger::info("HUD mode flags OWNED by the minimap clip: {}", owned.empty() ? "<none>" : owned);
		logger::info("HUD mode flags MISSING: {}", missing.empty() ? "<none>" : missing);

		if (!displayObj.HasMember("All"))
		{
			logger::warn("The clip does not own an \"All\" property. ShowElements tests "
						 "hasOwnProperty(mode) and will hide this element every time it runs, even "
						 "though the mode stack reads \"All\" - which is exactly the observed flicker.");
		}
	}

	void Minimap::SetDisplayObjectVisible(bool a_visible)
	{
		if (!displayObj.IsDisplayObject())
		{
			static bool warnedNotDisplayObject = false;

			if (!warnedNotDisplayObject)
			{
				logger::warn("SetDisplayObjectVisible: displayObj is not a display object; visibility not set");
				warnedNotDisplayObject = true;
			}

			return;
		}

		RE::GFxValue::DisplayInfo displayInfo;

		if (!displayObj.GetDisplayInfo(&displayInfo))
		{
			static bool warnedNoInfo = false;

			if (!warnedNoInfo)
			{
				logger::warn("SetDisplayObjectVisible: could not read displayObj's display info; visibility not set");
				warnedNoInfo = true;
			}

			return;
		}

		displayInfo.SetVisible(a_visible);
		displayObj.SetDisplayInfo(displayInfo);

		logger::debug("SetDisplayObjectVisible: displayObj _visible set to {}", a_visible);
	}

	void Minimap::Show(bool a_persist)
	{
		logger::debug("Showing minimap (persist={})", a_persist);


		settings::display::showOnGameStart = true;

		// Keep the in-memory collection in step for anything that reads back through it, then
		// persist through settings::, not the collection's own WriteSetting. WriteSetting resolves
		// to the engine's implementation, which goes through the Win32 profile API - Mod Organizer
		// 2's usvfs does not reliably redirect those, so the write reported success and silently
		// never reached disk. See CLAUDE.md rule 16.
		auto iniSettingCollection = utils::INISettingCollection::GetSingleton();
		if (auto showOnGameStart = iniSettingCollection->GetSetting("bShowOnGameStart:Display"))
		{
			showOnGameStart->data.b = settings::display::showOnGameStart;
		}
		else
		{
			logger::warn("Setting \"bShowOnGameStart:Display\" is missing from the collection; the in-memory copy was not updated");
		}

		// Only a deliberate settings-page choice reaches the INI. A runtime toggle changes what
		// is on screen and nothing more.
		if (a_persist)
		{
			settings::SaveShowOnGameStart();
		}

		localMap_->inForeground = localMap_->enabled = true;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ true });

		// Show the clip this mod actually tests for visibility, not only the one it invokes
		// "Show" on. root and displayObj are different objects: Advance() gates all its per-frame
		// work on IsVisible(), which reads displayObj's own _visible, while this only ever told
		// root to show itself.
		//
		// On Skyrim SE that happened to work, because displayObj was already visible. On
		// Anniversary Edition 1.6.1170 it is not, so the minimap was never drawn at all while
		// every other path reported success - settings applied, positions computed, no errors.
		// Confirmed by the AE diagnostic build: IsShown=true, IsVisible=false, with displayObj
		// resolving fine and GetDisplayInfo succeeding.
		SetDisplayObjectVisible(true);
	}

	void Minimap::Hide(bool a_persist)
	{
		logger::debug("Hiding minimap (persist={})", a_persist);


		settings::display::showOnGameStart = false;

		// Keep the in-memory collection in step for anything that reads back through it, then
		// persist through settings::, not the collection's own WriteSetting. WriteSetting resolves
		// to the engine's implementation, which goes through the Win32 profile API - Mod Organizer
		// 2's usvfs does not reliably redirect those, so the write reported success and silently
		// never reached disk. See CLAUDE.md rule 16.
		auto iniSettingCollection = utils::INISettingCollection::GetSingleton();
		if (auto showOnGameStart = iniSettingCollection->GetSetting("bShowOnGameStart:Display"))
		{
			showOnGameStart->data.b = settings::display::showOnGameStart;
		}
		else
		{
			logger::warn("Setting \"bShowOnGameStart:Display\" is missing from the collection; the in-memory copy was not updated");
		}

		// Only a deliberate settings-page choice reaches the INI. A runtime toggle changes what
		// is on screen and nothing more.
		if (a_persist)
		{
			settings::SaveShowOnGameStart();
		}

		localMap_->inForeground = localMap_->enabled = false;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ false });

		// Mirror of Show() - hide the clip visibility is actually tested on.
		SetDisplayObjectVisible(false);
	}
}
