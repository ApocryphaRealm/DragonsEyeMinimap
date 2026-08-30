#include "Minimap.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"

#include <algorithm>

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
			if (miniMap->inputControlledMode) { miniMap->LeaveInputControlledMode(); }
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
			// Keyboard: the hide/zoom keys. Mouse: the wheel, only while the map is being held
			// (hold-to-pan, 1.5.9) - otherwise the wheel belongs to the camera as usual.
			if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard ||
				(buttonEvent->GetDevice() == RE::INPUT_DEVICE::kMouse && miniMap->inputControlledMode))
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
		// Hold-to-pan on the hide key (1.5.9, the author). Mirrors the original mod's scheme: a press
		// shorter than fHoldToPanSecs toggles hide/show on RELEASE; holding past it hands the
		// mouse to the map (move = pan, wheel = zoom) until the key is released. The game keeps
		// sending the button event every frame while it is held, with heldDownSecs climbing.
		if (settings::controls::hideKeyCode > 0 &&
			a_buttonEvent->GetIDCode() == static_cast<std::uint32_t>(settings::controls::hideKeyCode) &&
			a_buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
		{
			const bool isPressed = a_buttonEvent->Value() != 0.0F;
			const bool isReleased = !isPressed;
			const float held = a_buttonEvent->GetRuntimeData().heldDownSecs;
			const float threshold = std::max(0.05F, settings::controls::holdToPanSecs);

			if (isReleased && held < threshold)
			{
				logger::debug("Hide key tapped ({}s) - minimap now {}", held, miniMap->IsShown() ? "hidden" : "shown");
				// Runtime toggle: change what is drawn, leave bShowOnGameStart alone.
				miniMap->IsShown() ? miniMap->Hide(false) : miniMap->Show(false);
			}
			else if (isPressed && held >= threshold && miniMap->IsShown())
			{
				if (!miniMap->inputControlledMode) { miniMap->EnterInputControlledMode(); }
			}
			if (isReleased && miniMap->inputControlledMode)
			{
				miniMap->LeaveInputControlledMode();
			}
			return true;
		}

		// Wheel zoom while holding: the map's own zoom channel, at the game's local-map speed.
		if (miniMap->inputControlledMode && a_buttonEvent->GetDevice() == RE::INPUT_DEVICE::kMouse && a_buttonEvent->IsDown())
		{
			auto* controlMap = RE::ControlMap::GetSingleton();
			auto* userEvents = RE::UserEvents::GetSingleton();
			if (controlMap && userEvents && miniMap->cameraContext)
			{
				const std::string_view name = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), RE::INPUT_DEVICE::kMouse, RE::ControlMap::InputContextID::kMap);
				if (name == userEvents->zoomIn) { miniMap->cameraContext->zoomInput += miniMap->localMapMouseZoomSpeed; return true; }
				if (name == userEvents->zoomOut) { miniMap->cameraContext->zoomInput -= miniMap->localMapMouseZoomSpeed; return true; }
			}
		}

		if (a_buttonEvent->IsDown())
		{
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

	bool Minimap::InputHandler::ProcessMouseMove(RE::MouseMoveEvent* a_event)
	{
		if (!miniMap->inputControlledMode || !a_event || !miniMap->cameraContext || !miniMap->cameraContext->defaultState || !miniMap->cameraContext->cameraRoot)
		{
			return false;
		}
		const float xOffset = -a_event->mouseInputX * miniMap->localMapMousePanSpeed;
		const float yOffset = a_event->mouseInputY * miniMap->localMapMousePanSpeed;
		const RE::NiPoint3 translationOffset = miniMap->cameraContext->cameraRoot->local.rotate * RE::NiPoint3{ 0, yOffset, xOffset };
		miniMap->cameraContext->defaultState->translation += translationOffset;
		return true;
	}

	void Minimap::EnterInputControlledMode()
	{
		inputControlledMode = true;
		if (auto* controlMap = RE::ControlMap::GetSingleton())
		{
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, false);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, false);
		}
		logger::debug("hold-to-pan: entered (looking + wheel zoom handed to the map)");
	}

	void Minimap::LeaveInputControlledMode()
	{
		inputControlledMode = false;
		if (auto* controlMap = RE::ControlMap::GetSingleton())
		{
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, true);
		}
		logger::debug("hold-to-pan: left (controls returned to the camera)");
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
