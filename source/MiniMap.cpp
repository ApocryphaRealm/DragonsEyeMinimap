#include "Minimap.h"

#include <numbers>

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

			if (settings::display::showOnGameStart)
			{
				Show();
			}
			else
			{
				Hide();
			}
		}
	}

	void Minimap::MeasureStage()
	{
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

		logger::info("Minimap stage: {}x{}", stageWidth, stageHeight);

		if (stageWidth <= 0.0F || stageHeight <= 0.0F)
		{
			logger::error("Could not read the Stage size; the minimap cannot be positioned");
		}
	}

	bool Minimap::StageToParent(float a_stageX, float a_stageY, float& a_outX, float& a_outY)
	{
		auto* view = displayObj.GetMovieView();
		if (!view)
		{
			return false;
		}

		// IUI::GFxObject::GetMember hides the two-argument RE version, so use the wrapper's
		// single-argument form.
		RE::GFxValue parent = displayObj.GetMember("_parent");
		if (!parent.IsDisplayObject())
		{
			return false;
		}

		// globalToLocal rewrites the point object in place. This mirrors the idiom the mod's
		// own ActionScript already uses in LocalMap.as InitMap, which converts the other way
		// with _parent.localToGlobal.
		IUI::GFxObject point(view);
		point.SetMember("x", RE::GFxValue{ static_cast<double>(a_stageX) });
		point.SetMember("y", RE::GFxValue{ static_cast<double>(a_stageY) });

		RE::GFxValue argument = point;
		if (!parent.Invoke("globalToLocal", nullptr, &argument, 1))
		{
			return false;
		}

		a_outX = static_cast<float>(point.GetMember("x").GetNumber());
		a_outY = static_cast<float>(point.GetMember("y").GetNumber());

		return true;
	}

	bool Minimap::GetArtBoundsInParent(float& a_left, float& a_top, float& a_right, float& a_bottom)
	{
		// IUI::GFxObject::GetMember hides the two-argument RE version, so use the wrapper's
		// single-argument form.
		RE::GFxValue parent = displayObj.GetMember("_parent");
		if (!parent.IsDisplayObject())
		{
			return false;
		}

		// Asking for the bounds *in the parent's space* folds in the clip's own position and
		// scale, so the result is directly comparable with _x and _y. Measuring afresh each
		// time also means it does not matter that the map contents are attached later than
		// this object is constructed.
		RE::GFxValue bounds;
		if (!displayObj.Invoke("getBounds", &bounds, parent) || !bounds.IsObject())
		{
			return false;
		}

		RE::GFxValue xMin, xMax, yMin, yMax;
		bounds.GetMember("xMin", &xMin);
		bounds.GetMember("xMax", &xMax);
		bounds.GetMember("yMin", &yMin);
		bounds.GetMember("yMax", &yMax);

		a_left = static_cast<float>(xMin.GetNumber());
		a_top = static_cast<float>(yMin.GetNumber());
		a_right = static_cast<float>(xMax.GetNumber());
		a_bottom = static_cast<float>(yMax.GetNumber());

		return a_right > a_left && a_bottom > a_top;
	}

	float Minimap::GetMaxScale() const
	{
		// A quarter of the screen means one quadrant: half its width and half its height.
		if (stageWidth <= 0.0F || stageHeight <= 0.0F || artWidthAtScaleOne <= 0.0F || artHeightAtScaleOne <= 0.0F)
		{
			return settings::display::kScaleSliderMax;
		}

		const float byWidth = stageWidth * 0.5F / artWidthAtScaleOne;
		const float byHeight = stageHeight * 0.5F / artHeightAtScaleOne;

		return std::clamp(std::min({ byWidth, byHeight, settings::display::kScaleSliderMax }),
						  settings::display::kScaleSliderMin, settings::display::kScaleSliderMax);
	}

	void Minimap::ApplyDisplaySettings()
	{
		if (!displayObj.HasMember("Minimap") || stageWidth <= 0.0F || stageHeight <= 0.0F)
		{
			return;
		}

		const float scale = std::clamp(settings::display::scale, settings::display::kScaleSliderMin, GetMaxScale());

		displayObj.SetMember("_xscale", baseXScale * scale);
		displayObj.SetMember("_yscale", baseYScale * scale);

		float artLeft = 0.0F, artTop = 0.0F, artRight = 0.0F, artBottom = 0.0F;
		if (!GetArtBoundsInParent(artLeft, artTop, artRight, artBottom))
		{
			logger::error("Could not measure the minimap artwork; leaving it where it is");

			return;
		}

		// Remember how big the artwork is at scale 1, so the quarter-screen cap has something
		// real to work from rather than a value guessed before the map was attached.
		if (scale > 0.0F)
		{
			artWidthAtScaleOne = (artRight - artLeft) / scale;
			artHeightAtScaleOne = (artBottom - artTop) / scale;
		}

		using Anchor = settings::display::Anchor;
		const auto anchor = static_cast<Anchor>(settings::display::AnchorIndex());

		const bool atRight = anchor == Anchor::kTopRight || anchor == Anchor::kBottomRight;
		const bool atBottom = anchor == Anchor::kBottomLeft || anchor == Anchor::kBottomRight;

		// Where the artwork's chosen corner should end up, in stage pixels.
		const float wantStageX = (atRight ? stageWidth : 0.0F) + settings::display::offsetX;
		const float wantStageY = (atBottom ? stageHeight : 0.0F) + settings::display::offsetY;

		float wantX = 0.0F, wantY = 0.0F;
		if (!StageToParent(wantStageX, wantStageY, wantX, wantY))
		{
			logger::error("Could not reach the minimap's parent clip; leaving it where it is");

			return;
		}

		// Move by the difference between where the chosen edge is and where it should be,
		// rather than computing an absolute _x. The registration point can sit anywhere
		// inside the artwork and this does not care where.
		const float currentX = static_cast<float>(displayObj.GetMember("_x").GetNumber());
		const float currentY = static_cast<float>(displayObj.GetMember("_y").GetNumber());

		float newX = currentX + (wantX - (atRight ? artRight : artLeft));
		float newY = currentY + (wantY - (atBottom ? artBottom : artTop));

		// Keep the whole artwork on screen, so raising the scale grows it inwards rather than
		// over the edge. The screen corners go through the same conversion as the target.
		float screenMinX = 0.0F, screenMinY = 0.0F, screenMaxX = 0.0F, screenMaxY = 0.0F;
		if (StageToParent(0.0F, 0.0F, screenMinX, screenMinY) &&
			StageToParent(stageWidth, stageHeight, screenMaxX, screenMaxY))
		{
			if (screenMaxX < screenMinX)
			{
				std::swap(screenMinX, screenMaxX);
			}
			if (screenMaxY < screenMinY)
			{
				std::swap(screenMinY, screenMaxY);
			}

			// artLeft/artTop were measured before the move, so express the limits relative to
			// the offset between the registration point and the artwork's edge.
			const float artWidth = artRight - artLeft;
			const float artHeight = artBottom - artTop;

			const float lowX = screenMinX + (currentX - artLeft);
			const float highX = screenMaxX - artWidth + (currentX - artLeft);
			newX = highX >= lowX ? std::clamp(newX, lowX, highX) : lowX;

			const float lowY = screenMinY + (currentY - artTop);
			const float highY = screenMaxY - artHeight + (currentY - artTop);
			newY = highY >= lowY ? std::clamp(newY, lowY, highY) : lowY;
		}

		displayObj.SetMember("_x", newX);
		displayObj.SetMember("_y", newY);

		// The map extents the renderer uses are worked out once, by the ActionScript InitMap,
		// from the clip's geometry at that moment. Moving or rescaling the clip afterwards
		// leaves them stale, so ask for them again.
		if (localMap_)
		{
			localMap_->root.Invoke("InitMap");
		}

		logger::debug("Display applied: anchor {}, offset ({}, {}), scale {} -> _x {}, _y {} "
					  "(art {},{} to {},{})",
					  settings::display::anchor, settings::display::offsetX, settings::display::offsetY,
					  scale, newX, newY, artLeft, artTop, artRight, artBottom);
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

		// Alternates deterministically rather than jumping to "whichever preset the camera is
		// currently further from": if the player has scrolled the map to a third value between
		// the two, distance-based picking can toggle back and forth between the same target,
		// or pick one arbitrarily depending on which side of the midpoint they landed on. A
		// remembered on/off state means a tap always does what it did last time.
		zoomedIn = !zoomedIn;

		const float target = zoomedIn ? settings::controls::zoomZoomedIn : settings::controls::zoomDefault;

		logger::info("Zoom toggle: camera reports {}, targeting {} ({})",
					 cameraContext->defaultState->zoom, target, zoomedIn ? "zoomed in" : "default");

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

				// Nothing writes this any more - panning was the only thing that did - but
				// zeroing it here still matters: it is what recenters the camera on the player
				// on the frame after a cell change, before Update() runs.
				cameraContext->defaultState->translation = RE::NiPoint3::Zero();

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

}
