#include "Minimap.h"

namespace DEM
{
	bool Minimap::InputHandler::CanProcess(RE::InputEvent* a_event)
	{
		auto ui = RE::UI::GetSingleton();
		return ui->menuStack.size() == 1 && ui->menuStack.front() == ui->GetMenu(RE::HUDMenu::MENU_NAME);
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

			miniMap->cameraContext->defaultState->translation += RE::NiPoint3(xOffset, yOffset, 0);
		}

		return true;
	}
	
	bool Minimap::InputHandler::ProcessButton(RE::ButtonEvent* a_event)
	{
		if (RE::ButtonEvent* buttonEvent = a_event->AsButtonEvent())
		{
			auto controlMap = RE::ControlMap::GetSingleton();
			auto userEvents = RE::UserEvents::GetSingleton();

			std::string_view userEventName = controlMap->GetUserEventName(buttonEvent->GetIDCode(), buttonEvent->GetDevice(), RE::ControlMap::InputContextID::kMap);

			if (userEventName == userEvents->localMap)
			{
				miniMap->inputControlledMode = buttonEvent->Value();

				RE::PlayerControls::GetSingleton()->lookHandler->SetInputEventHandlingEnabled(!miniMap->inputControlledMode);

				if (miniMap->inputControlledMode)
				{
					miniMap->UnfoldControls();
					controlMap->enabledControls.reset(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
				}
				else
				{
					miniMap->FoldControls();
					controlMap->enabledControls.set(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom);
				}
			}
			else if (miniMap->inputControlledMode)
			{
				bool isZoomIn = userEventName == userEvents->zoomIn;
				bool isZoomOut = userEventName == userEvents->zoomOut;

				bool isLocalMapMoveMode = userEventName == userEvents->localMapMoveMode;

				if (isZoomIn || isZoomOut)
				{
					float zoom;

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
			//else
			//{
			//	logger::info("ProcessButton (ButtonEvent): {} ({})", buttonEvent->GetIDCode(), buttonEvent->Value() ? "pressed" : "released");
			//}
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