#include "Minimap.h"

namespace DEM
{
	bool Minimap::InputHandler::CanProcess(RE::InputEvent* a_event)
	{
		if (RE::UI::GetSingleton()->GameIsPaused() || !miniMap->IsVisible())
		{
			miniMap->inputControlledMode = false;
			return false;
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessThumbstick(RE::ThumbstickEvent* a_event)
	{
		auto controlMap = RE::ControlMap::GetSingleton();
		auto userEvents = RE::UserEvents::GetSingleton();

		std::string_view gameplayUserEventName = controlMap->GetUserEventName(a_event->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kGameplay);
		std::string_view userEventName = controlMap->GetUserEventName(a_event->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kMap);

		if (miniMap->inputControlledMode)
		{
			if (userEventName == userEvents->look)
			{
				float xOffset = 2 * a_event->xValue * std::abs(a_event->xValue) * miniMap->localMapGamepadPanSpeed;
				float yOffset = 2 * a_event->yValue * std::abs(a_event->yValue) * miniMap->localMapGamepadPanSpeed;

				RE::NiPoint3 translationOffset = miniMap->cameraContext->cameraRoot->local.rotate * RE::NiPoint3{ 0, yOffset, xOffset };

				miniMap->cameraContext->defaultState->translation += translationOffset;
			}
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessMouseMove(RE::MouseMoveEvent* a_event)
	{
		if (miniMap->inputControlledMode)
		{
			float xOffset = -a_event->mouseInputX * miniMap->localMapMousePanSpeed;
			float yOffset = a_event->mouseInputY * miniMap->localMapMousePanSpeed;

			RE::NiPoint3 translationOffset = miniMap->cameraContext->cameraRoot->local.rotate * RE::NiPoint3{ 0, yOffset, xOffset };

			miniMap->cameraContext->defaultState->translation += translationOffset;
		}

		return true;
	}
	
	bool Minimap::InputHandler::ProcessButton(RE::ButtonEvent* a_event)
	{
		auto controls = RE::PlayerControls::GetSingleton();

		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			switch (buttonEvent->GetDevice())
			{
			case RE::INPUT_DEVICE::kKeyboard:
			case RE::INPUT_DEVICE::kMouse:
				return ProcessKeyboardOrMouseButton(buttonEvent);
			case RE::INPUT_DEVICE::kGamepad:
				return ProcessGamepadButton(buttonEvent);
			
			}
		}

		return false;
	}

	bool Minimap::InputHandler::ProcessKeyboardOrMouseButton(RE::ButtonEvent* a_buttonEvent)
	{
		auto controlMap = RE::ControlMap::GetSingleton();
		auto userEvents = RE::UserEvents::GetSingleton();

		std::string_view userEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), a_buttonEvent->GetDevice(), RE::ControlMap::InputContextID::kMap);

		if (userEventName == userEvents->localMap)
		{
			bool isPressed = a_buttonEvent->Value();
			bool isReleased = !isPressed;
			float heldDownSecs = a_buttonEvent->GetRuntimeData().heldDownSecs;

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
					if (!miniMap->inputControlledMode)
					{
						miniMap->inputControlledMode = true;

						controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, false);
						controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, false);

						miniMap->UnfoldControls();
					}
				}
				else
				{
					if (miniMap->inputControlledMode)
					{
						miniMap->inputControlledMode = false;

						controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);
						controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, true);

						miniMap->FoldControls();
					}
				}
			}
		}

		if (miniMap->inputControlledMode)
		{
			if (userEventName == userEvents->zoomIn)
			{
				miniMap->cameraContext->zoomInput += miniMap->localMapMouseZoomSpeed;
			}
			else if (userEventName == userEvents->zoomOut)
			{
				miniMap->cameraContext->zoomInput -= miniMap->localMapMouseZoomSpeed;
			}
		}

		return true;
	}

	bool Minimap::InputHandler::ProcessGamepadButton(RE::ButtonEvent* a_buttonEvent)
	{
		auto controlMap = RE::ControlMap::GetSingleton();
		auto userEvents = RE::UserEvents::GetSingleton();

		std::string_view gameplayUserEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kGameplay);

		if (gameplayUserEventName == userEvents->togglePOV)
		{
			bool isPressed = a_buttonEvent->Value();
			bool isReleased = !isPressed;
			float heldDownSecs = a_buttonEvent->GetRuntimeData().heldDownSecs;

			if (isReleased && heldDownSecs < settings::controls::holdDownToControlSecs)
			{
				miniMap->IsShown() ? miniMap->Hide() : miniMap->Show();
			}

			if (miniMap->IsShown())
			{
				if (isPressed && heldDownSecs >= settings::controls::holdDownToControlSecs)
				{
					if (!miniMap->inputControlledMode)
					{
						miniMap->inputControlledMode = true;

						controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, false);
						controlMap->ToggleControls(RE::ControlMap::UEFlag::kFighting, false);

						miniMap->UnfoldControls();
					}
				}
				else
				{
					if (miniMap->inputControlledMode)
					{
						miniMap->inputControlledMode = false;

						controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);
						controlMap->ToggleControls(RE::ControlMap::UEFlag::kFighting, true);

						miniMap->FoldControls();
					}
				}
			}
		}

		controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, !miniMap->IsShown());

		if (miniMap->inputControlledMode)
		{
			std::string_view mapUserEventName = controlMap->GetUserEventName(a_buttonEvent->GetIDCode(), RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kMap);

			if (mapUserEventName == userEvents->zoomIn)
			{
				miniMap->cameraContext->zoomInput += a_buttonEvent->Value() * miniMap->localMapGamepadZoomSpeed;
			}
			else if (mapUserEventName == userEvents->zoomOut)
			{
				miniMap->cameraContext->zoomInput -= a_buttonEvent->Value() * miniMap->localMapGamepadZoomSpeed;
			}
		}

		return true;
	}

	void Minimap::FoldControls()
	{
		RE::GFxValue controls;
		localMap_->root.GetMember("Controls", &controls);

		controls.GotoAndStop("Folded");
	}

	void Minimap::UnfoldControls()
	{
		RE::GFxValue controls;
		localMap_->root.GetMember("Controls", &controls);

		controls.GotoAndStop("Unfolded");
	}
}