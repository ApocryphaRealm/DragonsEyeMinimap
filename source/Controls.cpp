#include "Minimap.h"

#include "utils/INISettingCollection.h"

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
			if (isControllingMinimap)
			{
				StopControllingMinimap();
				miniMap->FoldControls();
			}

			if (registered)
			{
				menuControls->RemoveHandler(this);
			}

			return false;
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessThumbstick(RE::ThumbstickEvent* a_event)
	{
		if (isControllingMinimap)
		{
			std::string_view userEventName = controlMap->GetUserEventName(a_event->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kMap);

			if (userEventName == userEvents->look)
			{
				float xOffset = 2 * a_event->xValue * std::abs(a_event->xValue) * localMapGamepadPanSpeed;
				float yOffset = 2 * a_event->yValue * std::abs(a_event->yValue) * localMapGamepadPanSpeed;

				miniMap->ModTranslation(xOffset, yOffset);
			}
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessMouseMove(RE::MouseMoveEvent* a_event)
	{
		if (isControllingMinimap)
		{
			if (inputDeviceManager->IsGamepadEnabled())
			{
				return false;
			}

			float xOffset = -a_event->mouseInputX * localMapMousePanSpeed;
			float yOffset = a_event->mouseInputY * localMapMousePanSpeed;

			miniMap->ModTranslation(xOffset, yOffset);
		}

		return true;
	}
	
	bool Minimap::InputHandler::ProcessButton(RE::ButtonEvent* a_event)
	{
		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			bool isGamepadEnabled = inputDeviceManager->IsGamepadEnabled();

			switch (buttonEvent->GetDevice())
			{
			case RE::INPUT_DEVICE::kKeyboard:
			case RE::INPUT_DEVICE::kMouse:
				return !isGamepadEnabled ? 
					ProcessKeyboardOrMouseButton(buttonEvent) : false;
			case RE::INPUT_DEVICE::kGamepad:
				return isGamepadEnabled ?
					ProcessGamepadButton(buttonEvent) : false;
			}
		}

		return false;
	}

	bool Minimap::InputHandler::ProcessKeyboardOrMouseButton(RE::ButtonEvent* a_buttonEvent)
	{
		float buttonMag = a_buttonEvent->Value();

		std::string_view userEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), a_buttonEvent->GetDevice(), RE::ControlMap::InputContextID::kMap);

		if (userEventName == userEvents->localMap)
		{
			bool isPressed = buttonMag ? true : false;
			bool isReleased = !isPressed;
			float heldDownSecs = a_buttonEvent->HeldDuration();

			if (!miniMap->IsShown())
			{
				if (isReleased || (isPressed && heldDownSecs >= 2 * settings::controls::holdDownToControlSecs))
				{
					miniMap->Show();
				}
			}
			else
			{
				if (isReleased && heldDownSecs < settings::controls::holdDownToControlSecs)
				{
					miniMap->Hide();
				}
			}

			if (miniMap->IsShown())
			{
				if (isPressed && heldDownSecs >= settings::controls::holdDownToControlSecs)
				{
					if (!isControllingMinimap)
					{
						StartControllingMinimap();
						miniMap->UnfoldControls();
					}
				}
				else
				{
					if (isControllingMinimap)
					{
						StopControllingMinimap();
						miniMap->FoldControls();
					}

					miniMap->HideControlsAfter(settings::controls::delayToHideControlsSecs);
				}
			}
		}

		if (isControllingMinimap)
		{
			if (userEventName == userEvents->zoomIn)
			{
				miniMap->ModZoom(localMapMouseZoomSpeed);
			}
			else if (userEventName == userEvents->zoomOut)
			{
				miniMap->ModZoom(-localMapMouseZoomSpeed);
			}
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessGamepadButton(RE::ButtonEvent* a_buttonEvent)
	{
		float buttonMag = a_buttonEvent->Value();

		std::string_view gameplayUserEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kGameplay);

		if (gameplayUserEventName == userEvents->wait)
		{
			bool isPressed = buttonMag ? true : false;
			bool isReleased = !isPressed;
			float heldDownSecs = a_buttonEvent->HeldDuration();

			if (!miniMap->IsShown())
			{
				if (isReleased || (isPressed && heldDownSecs >= 2 * settings::controls::holdDownToControlSecs))
				{
					miniMap->Show();
				}
			}
			else
			{
				if (isReleased && heldDownSecs < settings::controls::holdDownToControlSecs)
				{
					miniMap->Hide();
				}
			}

			if (miniMap->IsShown())
			{
				if (isPressed && heldDownSecs >= settings::controls::holdDownToControlSecs)
				{
					if (!isControllingMinimap)
					{
						StartControllingMinimap();
						miniMap->UnfoldControls();
					}
				}
				else
				{
					if (isControllingMinimap)
					{
						StopControllingMinimap();
						miniMap->FoldControls();
					}

					miniMap->HideControlsAfter(settings::controls::delayToHideControlsSecs);
				}
			}
		}

		if (isControllingMinimap)
		{
			std::string_view mapUserEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kMap);

			if (mapUserEventName == userEvents->zoomIn)
			{
				miniMap->ModZoom(buttonMag * localMapGamepadZoomSpeed);
			}
			else if (mapUserEventName == userEvents->zoomOut)
			{
				miniMap->ModZoom(-buttonMag * localMapGamepadZoomSpeed);
			}
		}

		return true;
	}

	void Minimap::InputHandler::StartControllingMinimap()
	{
		isControllingMinimap = true;

		miniMap->ShowControls();

		controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, false);
		controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, false);

		if (inputDeviceManager->IsGamepadEnabled())
		{
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kFighting, false);
		}
	}

	void Minimap::InputHandler::StopControllingMinimap()
	{
		isControllingMinimap = false;

		miniMap->HideControlsAfter(settings::controls::delayToHideControlsSecs);

		controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, true);
		controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);

		if (inputDeviceManager->IsGamepadEnabled())
		{
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kFighting, true);
		}
	}

	void Minimap::Show()
	{
		settings::display::showOnGameStart = true;

		auto iniSettingCollection = utils::INISettingCollection::GetSingleton();
		if (auto showOnGameStart = iniSettingCollection->GetSetting("bShowOnGameStart:Display"))
		{
			showOnGameStart->data.b = settings::display::showOnGameStart;
			iniSettingCollection->WriteSetting(showOnGameStart);
		}

		localMap_->inForeground = localMap_->enabled = true;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ true });
		ShowControls();
	}

	void Minimap::Hide()
	{
		settings::display::showOnGameStart = false;

		auto iniSettingCollection = utils::INISettingCollection::GetSingleton();
		if (auto showOnGameStart = iniSettingCollection->GetSetting("bShowOnGameStart:Display"))
		{
			showOnGameStart->data.b = settings::display::showOnGameStart;
			iniSettingCollection->WriteSetting(showOnGameStart);
		}

		localMap_->inForeground = localMap_->enabled = false;
		localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ false });
	}

	void Minimap::ShowControls()
	{
		localMap_->root.Invoke("ShowControls");
	}

	void Minimap::HideControlsAfter(float a_delaySecs)
	{
		localMap_->root.Invoke("HideControls", std::array<RE::GFxValue, 1>{ a_delaySecs });
	}

	void Minimap::FoldControls()
	{
		localMap_->root.Invoke("FoldControls", std::array<RE::GFxValue, 1>{ settings::display::controlHideTip });
	}

	void Minimap::UnfoldControls()
	{
		localMap_->root.Invoke("UnfoldControls", std::array<RE::GFxValue, 2>{ settings::display::controlMoveTip, settings::display::controlZoomTip });
	}
}