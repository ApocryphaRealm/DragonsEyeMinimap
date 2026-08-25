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
		if (RE::UI__IsInMenuMode() || !miniMap->IsVisible())
		{
			if (registered)
			{
				logger::debug("Minimap no longer eligible to process input (menu open or minimap hidden); deregistering input handler");
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

				miniMap->IsShown() ? miniMap->Hide() : miniMap->Show();

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

	void Minimap::Show()
	{
		logger::debug("Showing minimap; persisting bShowOnGameStart:Display = true");

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

		settings::SaveShowOnGameStart();

		localMap_->inForeground = localMap_->enabled = true;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ true });
	}

	void Minimap::Hide()
	{
		logger::debug("Hiding minimap; persisting bShowOnGameStart:Display = false");

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

		settings::SaveShowOnGameStart();

		localMap_->inForeground = localMap_->enabled = false;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ false });
	}
}
