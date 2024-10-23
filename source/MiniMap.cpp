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

			// Init custom controls ///////////////////////////////////////////////////////////////////
			RE::MenuControls::GetSingleton()->RemoveHandler(localMap_->inputHandler.get());
			RE::MenuControls::GetSingleton()->AddHandler(inputHandler.get());

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

			Show();
		}
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
			localMap_->iconDisplay.Invoke("CreateMarkers", nullptr, nullptr, 0);
			PostCreateMarkers(localMap_->iconDisplay);
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

				if (!inputControlledMode)
				{
					cameraContext->defaultState->translation = RE::NiPoint3::Zero();
				}

				cameraContext->Update();

				isCameraUpdatePending = false;

				RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->GetRuntimeData2().loadedAreaBound;
				cameraContext->SetAreaBounds(loadedAreaBound->maxExtent, loadedAreaBound->minExtent);

				UpdateFogOfWar();
			}

			RenderOffscreen();
		}
	}

	void Minimap::RefreshPlatform()
	{
		bool gamepadConnected = RE::BSInputDeviceManager::GetSingleton()->IsGamepadConnected();
	}
}
