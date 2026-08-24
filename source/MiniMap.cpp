#include "Minimap.h"

#include <numbers>

namespace RE
{
	bool ControlMap__GetButtonNameFromUserEvent(ControlMap* a_this, const BSFixedString& a_eventID, INPUT_DEVICE a_device, ControlMap::InputContextID a_context, BSFixedString& a_buttonName)
	{
		if (auto gamepad = BSInputDeviceManager::GetSingleton()->GetGamepad())
		{
			if (const auto& inputContext = a_this->controlMap[a_context])
			{
				for (const auto& mapping : inputContext->deviceMappings[a_device])
				{
					if (mapping.eventID == a_eventID)
					{
						if (mapping.inputKey == 0xFF)
						{
							break;
						}

						for (auto& deviceButton : gamepad->buttonNameIDMap)
						{
							if (mapping.inputKey == static_cast<uint16_t>(deviceButton.second))
							{
								a_buttonName = deviceButton.first;
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}
}

namespace DEM
{
	bool Minimap::ProcessMessage(RE::UIMessage* a_message)
	{
		if (!localMap)
		{
			InitLocalMap();
		}

		if (localMap && !inputHandler->registered)
		{
			RE::MenuControls::GetSingleton()->AddHandler(inputHandler.get());
		}

		return false;
	}

	void Minimap::RegisterHUDComponent(RE::FxDelegateArgs& a_params)
	{
		RE::HUDObject::RegisterHUDComponent(a_params);
		displayObj.Invoke("AddToHudElements");
	}

	void Minimap::InitLocalMap()
	{
		localMap = static_cast<RE::LocalMapMenu*>(std::malloc(sizeof(RE::LocalMapMenu)));
		if (localMap)
		{
			localMap->Ctor();

			// Cache references ///////////////////////////////////////////////////////////////////////
			localMap_ = &localMap->GetRuntimeData();
			cullingProcess = &localMap->localCullingProcess;
			cameraContext = cullingProcess->GetLocalMapCamera();

			// Remove vanilla local map input controller
			RE::MenuControls::GetSingleton()->RemoveHandler(localMap_->inputHandler.get());

			// Set init state /////////////////////////////////////////////////////////////////////////
			localMap_->usingCursor = 0;

			cameraContext->currentState->Begin();

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
			cameraContext->SetDefaultStateInitialPosition(playerPos);

			// Init scaleform /////////////////////////////////////////////////////////////////////////

			// Set to reuse game logic
			localMap_->movieView = view.get();

			view->GetVariable(&localMap_->root, (std::string(DEM::Minimap::path) + ".MapClip").c_str());

			localMap_->root.Invoke("InitMap");
			localMap_->root.Invoke("SetShape", std::array<RE::GFxValue, 1>{ static_cast<std::uint32_t>(shape) });

			localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay);
			localMap_->iconDisplay.GetMember("MarkerData", &localMap->markerData);

			RefreshPlatform();

			if (settings::display::showOnGameStart)
			{
				Show();
				HideControlsAfter(settings::controls::delayToHideControlsSecs > 3.0F ? settings::controls::delayToHideControlsSecs : 3.0F);
			}
			else
			{
				Hide();
			}
		}
	}

	void Minimap::MeasurePositionMapping()
	{
		// The AS2 "Minimap" function positions the clip by running a stage coordinate through
		// globalToLocal, which is relative to the clip's own transform, and assigning the
		// result to _x/_y. That makes its output depend on where the clip already is, so
		// calling it repeatedly walks the minimap away from where it started and the same
		// setting stops meaning the same place.
		//
		// From a fixed starting state, though, the mapping from setting to _x/_y is affine:
		// the stage coordinate is linear in the setting, and globalToLocal is affine. So probe
		// it twice from the authored transform and keep the line. After this, position is
		// computed directly and the AS2 function is never called again, which is what makes a
		// setting mean one place no matter how it was arrived at - the same property the
		// anchor-and-offset scheme in other minimap mods gets for free.
		const auto probe = [this](float a_x, float a_y) {
			displayObj.SetMember("_x", baseX);
			displayObj.SetMember("_y", baseY);
			displayObj.SetMember("_xscale", baseXScale);
			displayObj.SetMember("_yscale", baseYScale);

			displayObj.Invoke("Minimap", a_x, a_y);

			return std::make_pair(static_cast<float>(displayObj.GetMember("_x").GetNumber()),
								  static_cast<float>(displayObj.GetMember("_y").GetNumber()));
		};

		const auto [zeroX, zeroY] = probe(0.0F, 0.0F);
		const auto [oneX, oneY] = probe(1.0F, 1.0F);

		positionOriginX = zeroX;
		positionOriginY = zeroY;
		positionSpanX = oneX - zeroX;
		positionSpanY = oneY - zeroY;

		// A zero span would mean every setting maps to the same place, which cannot be right
		// and would silently pin the minimap to one spot.
		hasPositionMapping = std::abs(positionSpanX) > 0.001F && std::abs(positionSpanY) > 0.001F;

		if (!hasPositionMapping)
		{
			logger::error("Could not measure the minimap position mapping (span {} x {}); "
						  "falling back to the stateful Scaleform positioning",
						  positionSpanX, positionSpanY);
		}
		else
		{
			logger::info("Minimap position mapping: origin ({}, {}), span ({}, {})",
						 positionOriginX, positionOriginY, positionSpanX, positionSpanY);
		}
	}

	void Minimap::ApplyDisplaySettings()
	{
		if (!displayObj.HasMember("Minimap"))
		{
			return;
		}

		if (hasPositionMapping)
		{
			// Absolute, so it depends only on the setting - never on how many times a slider
			// has moved or what the clip was doing beforehand.
			displayObj.SetMember("_x", positionOriginX + positionSpanX * settings::display::positionX);
			displayObj.SetMember("_y", positionOriginY + positionSpanY * settings::display::positionY);
		}
		else
		{
			displayObj.SetMember("_x", baseX);
			displayObj.SetMember("_y", baseY);
			displayObj.SetMember("_xscale", baseXScale);
			displayObj.SetMember("_yscale", baseYScale);

			displayObj.Invoke("Minimap", settings::display::positionX, settings::display::positionY);
		}

		// Scale through _xscale/_yscale rather than _width/_height. The latter are derived from
		// the clip's bounding box, which changes as children come and go, so the same _width
		// stops meaning the same scale over time.
		displayObj.SetMember("_xscale", baseXScale * settings::display::scale);
		displayObj.SetMember("_yscale", baseYScale * settings::display::scale);

		logger::debug("Display settings applied: position ({}, {}), scale {} -> _x {}, _y {}, _xscale {}",
					  settings::display::positionX, settings::display::positionY, settings::display::scale,
					  displayObj.GetMember("_x").GetNumber(), displayObj.GetMember("_y").GetNumber(),
					  displayObj.GetMember("_xscale").GetNumber());
	}

	void Minimap::ApplyShapeSetting()
	{
		const Shape newShape = static_cast<Shape>(settings::display::shape);

		if (newShape == shape)
		{
			return;
		}

		shape = newShape;

		if (!localMap_)
		{
			return;
		}

		// The AS2 SetShape duplicates the chosen background art into a clip called
		// "backgroundArtMask", hands that to VisionCone.setMask, and hides the other shape's
		// art. It was written to run once. Run twice it leaves two problems behind:
		//
		//  - the previous duplicate is still on the display list, and once setMask has moved
		//    on to the new one it stops being a mask and starts being drawn, so the old shape
		//    reappears alongside the new one;
		//  - the art for the shape being switched *to* was hidden by the previous call, and
		//    SetShape only ever hides the alternative, never re-shows the chosen one, so the
		//    duplicate it takes as the new mask is invisible.
		//
		// Clearing the stale duplicate and un-hiding both arts first puts the clip back in the
		// state SetShape expects to find.
		RE::GFxValue staleMask;
		if (localMap_->root.GetMember("backgroundArtMask", &staleMask) && staleMask.IsDisplayObject())
		{
			staleMask.Invoke("removeMovieClip");
		}

		for (const char* artName : { "BackgroundArtSquare", "BackgroundArtCircle" })
		{
			RE::GFxValue art;
			if (localMap_->root.GetMember(artName, &art) && art.IsDisplayObject())
			{
				art.SetMember("_visible", RE::GFxValue{ true });
			}
		}

		localMap_->root.Invoke("SetShape", std::array<RE::GFxValue, 1>{ static_cast<std::uint32_t>(shape) });

		logger::debug("Shape set to {}", shape == Shape::kRound ? "round" : "squared");
	}

	void Minimap::SetMapZoom(float a_zoom)
	{
		if (!cameraContext)
		{
			return;
		}

		// Steer the absolute value through zoomInput, which is the same channel the pan/zoom
		// controls use, so the camera applies its own limits rather than us guessing at them.
		cameraContext->zoomInput += a_zoom - cameraContext->defaultState->zoom;
	}

	void Minimap::ToggleZoomPreset()
	{
		if (!cameraContext)
		{
			return;
		}

		const float current = cameraContext->defaultState->zoom;
		const float first = settings::controls::zoomPreset1;
		const float second = settings::controls::zoomPreset2;

		// Go to whichever preset we are further from, so a tap always visibly changes
		// something even if the zoom has drifted off both presets.
		const float target = std::abs(current - first) < std::abs(current - second) ? second : first;

		logger::debug("Zoom toggle: {} -> {}", current, target);

		SetMapZoom(target);
	}

	void Minimap::SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs)
	{
		float localLeft = a_delegateArgs[0].GetNumber();
		float localTop = a_delegateArgs[1].GetNumber();
		float localRight = a_delegateArgs[2].GetNumber();
		float localBottom = a_delegateArgs[3].GetNumber();

		float identityMat2D[2][3] = { { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } };

		RE::GPointF localTopLeft{ localLeft, localTop };
		localMap->topLeft = view->TranslateToScreen(localTopLeft, identityMat2D);

		RE::GPointF localBottomRight{ localRight, localBottom };
		localMap->bottomRight = view->TranslateToScreen(localBottomRight, identityMat2D);

		float aspectRatio = (localMap->bottomRight.x - localMap->topLeft.x) / (localMap->bottomRight.y - localMap->topLeft.y);

		cameraContext->defaultState->minFrustumHalfWidth = aspectRatio * cameraContext->defaultState->minFrustumHalfHeight;

		minCamFrustumHalfWidth = cameraContext->defaultState->minFrustumHalfWidth;
		minCamFrustumHalfHeight = cameraContext->defaultState->minFrustumHalfHeight;
	}

	void Minimap::Advance()
	{
		if (IsVisible() && IsShown())
		{
			RE::GFxValue updateScaleform = displayObj.GetMember("updateScaleform");

			if (updateScaleform.GetBool())
			{
				updateScaleform.SetBoolean(false);

				std::array<RE::GFxValue, 2> title;

				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
				float playerCameraRotation = RE::PlayerCamera::GetSingleton()->GetRuntimeData2().yaw;

				float cellNorthRotation = 0.0F;

				if (RE::TESObjectCELL* parentCell = player->parentCell)
				{
					cellNorthRotation = -parentCell->GetNorthRotation();

					if (settings::controls::followPlayerCameraRotation)
					{
						cameraContext->SetNorthRotation(playerCameraRotation);
					}
					else
					{
						cameraContext->SetNorthRotation(cellNorthRotation);
					}
					cameraContext->zRotation = 0.0F;

					if (parentCell->IsInteriorCell())
					{
						title[0] = parentCell->GetFullName();
					}
					else if (RE::BGSLocation* location = parentCell->GetLocation())
					{
						title[0] = location->GetFullName();
					
						if (location->everCleared)
						{
							title[1] = clearedStr;
						}
					}
					else
					{
						RE::TESWorldSpace* worldSpace = player->GetWorldspace();
						title[0] = worldSpace->GetFullName();
					}
				}

				localMap_->root.Invoke("SetTitle", nullptr, title);
				
				localMap->PopulateData();
				localMap_->iconDisplay.Invoke("CreateMarkers");
				localMap->RefreshMarkers();
				
				if (settings::controls::followPlayerCameraRotation)
				{
					RE::GFxValue youAreHereMarker;
					localMap_->iconDisplay.GetMember("YouAreHereMarker", &youAreHereMarker);
				
					float playerToCamAngle = player->GetAngleZ() - playerCameraRotation;
					float playerToCamAngleDeg = playerToCamAngle * 180 * std::numbers::inv_pi;
				
					RE::GFxValue::DisplayInfo youAreHereMarkerDisplayInfo;
					youAreHereMarker.GetDisplayInfo(&youAreHereMarkerDisplayInfo);
					youAreHereMarkerDisplayInfo.SetRotation(playerToCamAngleDeg);
					youAreHereMarker.SetDisplayInfo(youAreHereMarkerDisplayInfo);
				}
				else
				{
					RE::GFxValue visionCone;
					localMap_->root.GetMember("VisionCone", &visionCone);
				
					float playerCameraToNorthAngle = playerCameraRotation - cellNorthRotation;
					float playerCameraToNorthAngleDeg = playerCameraToNorthAngle * 180 * std::numbers::inv_pi;
				
					RE::GFxValue::DisplayInfo visionConeDisplayInfo;
					visionCone.GetDisplayInfo(&visionConeDisplayInfo);
					visionConeDisplayInfo.SetRotation(playerCameraToNorthAngleDeg);
					visionCone.SetDisplayInfo(visionConeDisplayInfo);
				}

			}

			isCameraUpdatePending = true;
		}
	}

	void Minimap::PreRender()
	{
		if (IsVisible() && IsShown())
		{
			if (isCameraUpdatePending)
			{
				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

				RE::NiPoint3 playerPos = player->GetPosition();
				cameraContext->defaultState->initialPosition.x = playerPos.x;
				cameraContext->defaultState->initialPosition.y = playerPos.y;

				if (!inputHandler->IsControllingMinimap())
				{
					cameraContext->defaultState->translation = RE::NiPoint3::Zero();
				}

				cameraContext->Update();

				isCameraUpdatePending = false;

				RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->GetRuntimeData2().loadedAreaBound;
				cameraContext->SetAreaBounds(loadedAreaBound->maxExtent, loadedAreaBound->minExtent);

				if (isFogOfWarEnabled)
				{
					UpdateFogOfWar();
				}

				RenderOffScreen();
			}
		}
	}

	void Minimap::RefreshPlatform()
	{
		if (localMap)
		{
			auto controlMap = RE::ControlMap::GetSingleton();
			auto userEvents = RE::UserEvents::GetSingleton();

			RE::BSFixedString controlButton;
			RE::BSFixedString moveButton;
			RE::BSFixedString zoomInButton;
			RE::BSFixedString zoomOutButton;

			RE::GFxValue pcControlButtons;
			localMap_->root.GetMember("pcControlButtons", &pcControlButtons);
			pcControlButtons.ClearElements();

			controlMap->GetButtonNameFromUserEvent(userEvents->localMap, RE::INPUT_DEVICE::kKeyboard, controlButton);
			pcControlButtons.PushBack(RE::GFxValue{ controlButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->look, RE::INPUT_DEVICE::kMouse, moveButton);
			pcControlButtons.PushBack(RE::GFxValue{ moveButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->zoomIn, RE::INPUT_DEVICE::kMouse, zoomInButton);
			pcControlButtons.PushBack(RE::GFxValue{ zoomInButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->zoomOut, RE::INPUT_DEVICE::kMouse, zoomOutButton);
			pcControlButtons.PushBack(RE::GFxValue{ zoomOutButton.c_str() });

			RE::GFxValue gamepadControlButtons;
			localMap_->root.GetMember("gamepadControlButtons", &gamepadControlButtons);
			gamepadControlButtons.ClearElements();
			
			ControlMap__GetButtonNameFromUserEvent(controlMap, userEvents->wait, RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kGameplay, controlButton);
			gamepadControlButtons.PushBack(RE::GFxValue{ controlButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->look, RE::INPUT_DEVICE::kGamepad, moveButton);
			gamepadControlButtons.PushBack(RE::GFxValue{ moveButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->zoomIn, RE::INPUT_DEVICE::kGamepad, zoomInButton);
			gamepadControlButtons.PushBack(RE::GFxValue{ zoomInButton.c_str() });

			controlMap->GetButtonNameFromUserEvent(userEvents->zoomOut, RE::INPUT_DEVICE::kGamepad, zoomOutButton);
			gamepadControlButtons.PushBack(RE::GFxValue{ zoomOutButton.c_str() });

			bool isGamepadEnabled = RE::BSInputDeviceManager::GetSingleton()->IsGamepadEnabled();

			localMap_->root.Invoke("SetPlatform", std::array<RE::GFxValue, 2>{ isGamepadEnabled, false });

			FoldControls();
			ShowControls();
			HideControlsAfter(settings::controls::delayToHideControlsSecs < 1.5F ? 1.5F : settings::controls::delayToHideControlsSecs);
		}
	}
}
