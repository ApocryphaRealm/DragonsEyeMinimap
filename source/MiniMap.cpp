#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/R/RenderPassCache.h"

#include "DebugWidget.h"

namespace debug
{
	std::uint32_t GetCurrentNumOfUsedPasses();
}

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
			localMap_ = &localMap->GetRuntimeData();
			cullingProcess = &localMap->localCullingProcess;
			cameraContext = cullingProcess->GetLocalMapCamera();

			RE::MenuControls::GetSingleton()->RemoveHandler(localMap_->inputHandler.get());
			RE::MenuControls::GetSingleton()->AddHandler(inputHandler.get());

			localMap_->movieView = view.get();

			view->GetVariable(&localMap_->root, (std::string(DEM::Minimap::path) + ".MapClip").c_str());

			localMap_->root.Invoke("InitMap");

			view->CreateArray(&localMap->markerData);
			localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay);
			localMap_->iconDisplay.SetMember("MarkerData", localMap->markerData);

			localMap_->enabled = true;
			localMap_->usingCursor = 0;
			localMap_->inForeground = localMap_->enabled;

			view->CreateArray(&extraMarkerData);
			localMap_->iconDisplay.SetMember("ExtraMarkerData", extraMarkerData);

			cameraContext->currentState->Begin();

			RE::GFxValue gfxEnable = localMap_->enabled;
			localMap_->root.Invoke("Show", nullptr, &gfxEnable, 1);

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
			cameraContext->SetDefaultStateInitialPosition(playerPos);
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

	void Minimap::PreRender()
	{
		if (localMap && localMap_->enabled)
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

		std::uint32_t usedPasses = debug::GetCurrentNumOfUsedPasses();

		DebugWidget::GetSingleton()->SetRenderPasses(usedPasses);
	}

	void Minimap::Advance()
	{
		if (localMap && localMap_->enabled)
		{
			std::array<RE::GFxValue, 2> title;

			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

			if (RE::TESObjectCELL* parentCell = player->parentCell)
			{
				float cellNorthRotation = parentCell->GetNorthRotation();

				if (settings::controls::followPlayerCameraRotation)
				{
					float playerNorthRotation = -RE::PlayerCamera::GetSingleton()->GetRuntimeData2().yaw;
					cameraContext->SetNorthRotation(-playerNorthRotation);
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

			CreateMarkers();
			RefreshMarkers();

			isCameraUpdatePending = true;
		}
	}

	void Minimap::CreateMarkers()
	{
		localMap->PopulateData();

		enemyActors.clear();
		hostileActors.clear();
		guardActors.clear();

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		for (RE::ActorHandle& highActorHandle : RE::ProcessLists::GetSingleton()->highActorHandles)
		{
			RE::NiPointer<RE::Actor> actor = highActorHandle.get();
			if (actor && RE::NiCamera::PointInFrustum(actor->GetPosition(), cameraContext->camera.get(), 1))
			{
				bool isActorCombatant = false;

				for (RE::ActorHandle& combatantActorHandle : player->GetPlayerRuntimeData().actorsToDisplayOnTheHUDArray)
				{
					if (highActorHandle == combatantActorHandle)
					{
						enemyActors.push_back(actor);
						isActorCombatant = true;
						break;
					}
				}

				if (!isActorCombatant && !actor->IsDead() &&
					actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAggression) != 0.0F)
				{
					if (actor->IsHostileToActor(player))
					{
						hostileActors.push_back(actor);
					}
					else if (actor->IsGuard())
					{
						guardActors.push_back(actor);
					}
				}
			}
		}

		extraMarkerData.ClearElements();

		std::size_t numEnemyActors = enemyActors.size();
		std::size_t numHostileActors = hostileActors.size();
		std::size_t numGuardActors = guardActors.size();

		extraMarkerData.SetArraySize((numEnemyActors + numHostileActors + numGuardActors) * ExtraMarker::CreateData::kStride);

		int j = 0;

		for (int i = 0; i < numHostileActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = hostileActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kHostile);
			j += ExtraMarker::CreateData::kStride;
		}

		for (int i = 0; i < numGuardActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = guardActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kGuard);
			j += ExtraMarker::CreateData::kStride;
		}

		for (int i = 0; i < numEnemyActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = enemyActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kCombatant);
			j += ExtraMarker::CreateData::kStride;
		}

		localMap_->iconDisplay.Invoke("CreateMarkers", nullptr, nullptr, 0);
	}

	void Minimap::RefreshMarkers()
	{
		std::size_t numEnemyActors = enemyActors.size();
		std::size_t numHostileActors = hostileActors.size();
		std::size_t numGuardActors = guardActors.size();

		extraMarkerData.SetArraySize((numEnemyActors + numHostileActors + numGuardActors) * ExtraMarker::RefreshData::kStride);

		int j = 0;

		for (int i = 0; i < numHostileActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = hostileActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		for (int i = 0; i < numGuardActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = guardActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		for (int i = 0; i < numEnemyActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = enemyActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		localMap->RefreshMarkers();

		if (settings::controls::followPlayerCameraRotation)
		{
			RE::GFxValue youAreHereMarker;
			localMap_->iconDisplay.GetMember("YouAreHereMarker", &youAreHereMarker);

			RE::GFxValue::DisplayInfo youAreHereMarkerDisplayInfo;
			youAreHereMarker.GetDisplayInfo(&youAreHereMarkerDisplayInfo);
			youAreHereMarkerDisplayInfo.SetRotation(0);
			youAreHereMarker.SetDisplayInfo(youAreHereMarkerDisplayInfo);
		}
	}
}
