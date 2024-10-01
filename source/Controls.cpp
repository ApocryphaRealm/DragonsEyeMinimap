#include "Minimap.h"

#include "utils/Logger.h"

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
		return true;
	}

	bool Minimap::InputHandler::ProcessMouseMove(RE::MouseMoveEvent* a_event)
	{
		if (miniMap->inputControlledMode)
		{
			float xOffset = -a_event->mouseInputX * miniMap->localMapPanSpeed;
			float yOffset = a_event->mouseInputY * miniMap->localMapPanSpeed;

			RE::NiPoint3 translationOffset = miniMap->cameraContext->cameraRoot->local.rotate * RE::NiPoint3(0, yOffset, xOffset);

			miniMap->cameraContext->defaultState->translation += translationOffset;
		}

		return true;
	}
	
	bool Minimap::InputHandler::ProcessButton(RE::ButtonEvent* a_event)
	{
		auto controls = RE::PlayerControls::GetSingleton();

		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			auto controlMap = RE::ControlMap::GetSingleton();
			auto userEvents = RE::UserEvents::GetSingleton();

			std::uint32_t localMapKey = controlMap->GetMappedKey(userEvents->localMap, buttonEvent->GetDevice(), RE::UserEvents::INPUT_CONTEXT_IDS::kMap);

			if (buttonEvent->GetIDCode() == localMapKey)
			{
				bool isPressed = buttonEvent->Value();
				bool isReleased = !isPressed;
				float heldDownSecs = buttonEvent->GetRuntimeData().heldDownSecs;

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

							RE::PlayerControls::GetSingleton()->lookHandler->SetInputEventHandlingEnabled(false);
							miniMap->UnfoldControls();
							if (REL::Module::IsAE())
							{
								controlMap->enabledControls.reset(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
							}
							else
							{
								controlMap->enabledControls.set(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
							}
						}
					}
					else
					{
						if (miniMap->inputControlledMode)
						{
							miniMap->inputControlledMode = false;

							RE::PlayerControls::GetSingleton()->lookHandler->SetInputEventHandlingEnabled(true);
							miniMap->FoldControls();
							if (REL::Module::IsAE())
							{
								controlMap->enabledControls.set(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
							}
							else
							{
								controlMap->enabledControls.reset(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
							}
						}
					}
				}
			}
			
			if (miniMap->inputControlledMode)
			{
				std::string_view userEventName = controlMap->GetUserEventName(buttonEvent->GetIDCode(), buttonEvent->GetDevice(), RE::ControlMap::InputContextID::kMap);

				bool isZoomIn = userEventName == userEvents->zoomIn;
				bool isZoomOut = userEventName == userEvents->zoomOut;

				bool isLocalMapMoveMode = userEventName == userEvents->localMapMoveMode;

				if (isZoomIn || isZoomOut)
				{
					float zoom = 1;

					if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kMouse)
					{
						zoom = miniMap->localMapMouseZoomSpeed;
					}
					else if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad)
					{
						zoom = miniMap->localMapGamepadZoomSpeed;
					}

					if (isZoomOut)
					{
						zoom *= -1;
					}

					miniMap->cameraContext->zoomInput += zoom;
				}
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