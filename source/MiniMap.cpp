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
		// calling it repeatedly walks the minimap away from where it started.
		//
		// From a fixed starting state the mapping from screen proportion to _x/_y is affine -
		// the stage coordinate is linear in the proportion, and globalToLocal is affine - so
		// probe it twice and keep the line. Afterwards the position is computed directly and
		// the AS2 function is never called again, which is what lets a corner and an offset
		// mean one place regardless of how the minimap got there.
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

		hasPositionMapping = std::abs(positionSpanX) > 0.001F && std::abs(positionSpanY) > 0.001F;

		// Where the artwork sits relative to the clip's registration point. Asking the clip
		// for its own bounds beats assuming the registration point is a particular corner.
		displayObj.SetMember("_xscale", baseXScale);
		displayObj.SetMember("_yscale", baseYScale);

		RE::GFxValue bounds = displayObj.Invoke("getBounds", displayObj);
		if (bounds.IsObject())
		{
			RE::GFxValue xMin, xMax, yMin, yMax;
			bounds.GetMember("xMin", &xMin);
			bounds.GetMember("xMax", &xMax);
			bounds.GetMember("yMin", &yMin);
			bounds.GetMember("yMax", &yMax);

			boundsLeft = static_cast<float>(xMin.GetNumber());
			boundsTop = static_cast<float>(yMin.GetNumber());
			boundsWidth = static_cast<float>(xMax.GetNumber()) - boundsLeft;
			boundsHeight = static_cast<float>(yMax.GetNumber()) - boundsTop;
		}

		// The offsets are in screen pixels, so we need the screen size in the same units the
		// AS2 side works in. If Stage is not reachable, fall back to treating one offset unit
		// as one unit of the clip's parent space, which is 1:1 with pixels for this HUD.
		if (auto* view = displayObj.GetMovieView())
		{
			RE::GFxValue width, height;
			if (view->GetVariable(&width, "Stage.width") && width.IsNumber())
			{
				stageWidth = static_cast<float>(width.GetNumber());
			}
			if (view->GetVariable(&height, "Stage.height") && height.IsNumber())
			{
				stageHeight = static_cast<float>(height.GetNumber());
			}
		}

		if (stageWidth <= 0.0F || stageHeight <= 0.0F)
		{
			stageWidth = std::abs(positionSpanX);
			stageHeight = std::abs(positionSpanY);

			logger::warn("Could not read Stage size; treating offsets as parent-space units");
		}

		// The probe returns the clip's own coordinate space while the bounds arithmetic works
		// in the parent's. The two coincide only while the artwork is authored at the origin
		// at scale 100, which it is - but say so out loud rather than leaving a silent
		// assumption for whoever re-authors the SWF.
		if (baseX != 0.0F || baseY != 0.0F || baseXScale != 100.0F || baseYScale != 100.0F)
		{
			logger::warn("Minimap clip is authored at ({}, {}) scale ({}, {}) rather than the origin at 100%; "
						 "anchoring and the edge margin assume otherwise and will be off",
						 baseX, baseY, baseXScale, baseYScale);
		}

		logger::info("Position mapping: origin ({}, {}), span ({}, {}), bounds ({}, {}) {}x{}, stage {}x{}",
					 positionOriginX, positionOriginY, positionSpanX, positionSpanY,
					 boundsLeft, boundsTop, boundsWidth, boundsHeight, stageWidth, stageHeight);

		if (!hasPositionMapping)
		{
			logger::error("Could not measure the minimap position mapping; falling back to the "
						  "stateful Scaleform positioning");
		}
	}

	float Minimap::GetMaxScale() const
	{
		// A quarter of the screen means one quadrant: half its width and half its height. Any
		// bigger and a corner-anchored minimap stops being a minimap.
		if (!hasPositionMapping || boundsWidth <= 0.0F || boundsHeight <= 0.0F ||
			baseXScale <= 0.0F || baseYScale <= 0.0F)
		{
			return settings::display::kScaleSliderMax;
		}

		const float byWidth = (std::abs(positionSpanX) * 0.5F) * 100.0F / (boundsWidth * baseXScale);
		const float byHeight = (std::abs(positionSpanY) * 0.5F) * 100.0F / (boundsHeight * baseYScale);

		// Never below the slider's lower end: std::clamp with lo > hi is undefined, and both
		// the menu and ApplyDisplaySettings clamp against this value.
		return std::clamp(std::min({ byWidth, byHeight, settings::display::kScaleSliderMax }),
						  settings::display::kScaleSliderMin, settings::display::kScaleSliderMax);
	}

	void Minimap::ApplyDisplaySettings()
	{
		if (!displayObj.HasMember("Minimap"))
		{
			return;
		}

		// Clamped here as well as in the menu, so a hand-edited INI cannot produce a minimap
		// that swallows the screen.
		const float scale = std::clamp(settings::display::scale, settings::display::kScaleSliderMin, GetMaxScale());

		// _xscale/_yscale rather than _width/_height: the latter are derived from the clip's
		// bounding box, which changes as children come and go, so the same _width stops
		// meaning the same scale over time.
		displayObj.SetMember("_xscale", baseXScale * scale);
		displayObj.SetMember("_yscale", baseYScale * scale);

		if (!hasPositionMapping)
		{
			displayObj.SetMember("_x", baseX);
			displayObj.SetMember("_y", baseY);
			displayObj.Invoke("Minimap", 0.5F, 0.5F);

			return;
		}

		using Anchor = settings::display::Anchor;
		const auto anchor = static_cast<Anchor>(settings::display::AnchorIndex());

		const bool atRight = anchor == Anchor::kTopRight || anchor == Anchor::kBottomRight;
		const bool atBottom = anchor == Anchor::kBottomLeft || anchor == Anchor::kBottomRight;

		// The screen, in the clip's parent space. The span can be negative, so do not assume
		// origin is the smaller edge.
		const float screenLeft = std::min(positionOriginX, positionOriginX + positionSpanX);
		const float screenRight = std::max(positionOriginX, positionOriginX + positionSpanX);
		const float screenTop = std::min(positionOriginY, positionOriginY + positionSpanY);
		const float screenBottom = std::max(positionOriginY, positionOriginY + positionSpanY);

		// Where the artwork sits relative to the registration point, at the scale in use.
		// Anchoring against the artwork rather than the registration point is what makes a
		// right or bottom corner usable at all.
		// getBounds reports the clip's own coordinate space, so converting to parent units
		// needs the whole scale that is actually applied, not just fScale.
		const float xFactor = baseXScale * scale / 100.0F;
		const float yFactor = baseYScale * scale / 100.0F;

		const float visualLeft = boundsLeft * xFactor;
		const float visualTop = boundsTop * yFactor;
		const float visualWidth = boundsWidth * xFactor;
		const float visualHeight = boundsHeight * yFactor;

		// One margin unit is one screen pixel, converted into parent space.
		const float unitX = std::abs(positionSpanX) / stageWidth;
		const float unitY = std::abs(positionSpanY) / stageHeight;
		const float marginX = settings::display::edgeMargin * unitX;
		const float marginY = settings::display::edgeMargin * unitY;

		float x = atRight ? screenRight - marginX - visualWidth - visualLeft
						  : screenLeft + marginX - visualLeft;
		float y = atBottom ? screenBottom - marginY - visualHeight - visualTop
						   : screenTop + marginY - visualTop;

		// Growing the scale must not push the minimap off the screen. Clamp the artwork's box
		// inside the screen rather than trusting the corner arithmetic, so a scale large
		// enough to overrun the margin still leaves the whole map visible. If it is larger
		// than the screen there is nothing to be done, so keep the top-left corner in view.
		const float maxX = screenRight - visualWidth - visualLeft;
		const float minX = screenLeft - visualLeft;
		x = maxX >= minX ? std::clamp(x, minX, maxX) : minX;

		const float maxY = screenBottom - visualHeight - visualTop;
		const float minY = screenTop - visualTop;
		y = maxY >= minY ? std::clamp(y, minY, maxY) : minY;

		displayObj.SetMember("_x", x);
		displayObj.SetMember("_y", y);

		logger::debug("Display applied: anchor {}, margin {}, scale {} -> _x {}, _y {} (art {}x{})",
					  settings::display::anchor, settings::display::edgeMargin, scale, x, y, visualWidth, visualHeight);
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
		if (!cameraContext || !cameraContext->defaultState)
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

		if (!cameraContext->defaultState)
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
