#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/R/RenderPassCache.h"

#include "DebugWidget.h"

namespace debug
{
	std::uint32_t GetCurrentNumOfUsedPasses()
	{
		RE::RenderPassCache* renderPassCache = RE::RenderPassCache::GetSingleton();

		RE::RenderPassCache::Pool& pool = renderPassCache->pools[0];

		uint32_t usedPasses = 0;
		static constexpr uint32_t passCount = 65535;

		for (uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
		{
			const RE::BSRenderPass& pass = pool.passes[passIndex];
			if (pass.passEnum != 0)
			{
				usedPasses++;
			}
		}

		return usedPasses;
	}
}

namespace RE
{
	std::int32_t TESObjectREFR_GetInventoryCount(TESObjectREFR* a_object, bool a_useDataHandlerInventory = false, bool a_unk03 = false)
	{
		using func_t = decltype(&TESObjectREFR_GetInventoryCount);
		REL::Relocation<func_t> func{ RELOCATION_ID(19274, 19700) };
		return func(a_object, a_useDataHandlerInventory, a_unk03);
	}

	std::int32_t ExtraDataList_GetDroppedWeapon(ExtraDataList* a_extraList, TESObjectREFRPtr& a_weapon)
	{
		using func_t = decltype(&ExtraDataList_GetDroppedWeapon);
		REL::Relocation<func_t> func{ RELOCATION_ID(11616, 11762) };
		return func(a_extraList, a_weapon);
	}

	std::int32_t ExtraDataList_GetDroppedUtil(ExtraDataList* a_extraList, TESObjectREFRPtr& a_util)
	{
		using func_t = decltype(&ExtraDataList_GetDroppedUtil);
		REL::Relocation<func_t> func{ RELOCATION_ID(11617, 11763) };
		return func(a_extraList, a_util);
	}

	bool TESObjectREFR_HasAnyDroppedItem(TESObjectREFR* a_object)
	{
		if (std::int32_t inventoryCount = RE::TESObjectREFR_GetInventoryCount(a_object))
		{
			return true;
		}
		else
		{
			if (a_object->formType == RE::FormType::ActorCharacter)
			{
				RE::TESObjectREFRPtr carriedDroppedWeapon;
				RE::ExtraDataList_GetDroppedWeapon(&a_object->extraList, carriedDroppedWeapon);
				if (carriedDroppedWeapon)
				{
					return true;
				}

				RE::TESObjectREFRPtr carriedDroppedUtil;
				RE::ExtraDataList_GetDroppedUtil(&a_object->extraList, carriedDroppedUtil);
				if (carriedDroppedUtil)
				{
					return true;
				}
			}

			return false;
		}
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

			RE::GFxValue altBackgroundShape;
			localMap_->root.GetMember(shape == Shape::kCircle ? "BackgroundArtSquare" : "BackgroundArtCircle", &altBackgroundShape);

			if (altBackgroundShape.IsDisplayObject())
			{
				altBackgroundShape.SetMember("_visible", false);
			}

			view->CreateArray(&localMap->markerData);
			localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay);
			localMap_->iconDisplay.SetMember("MarkerData", localMap->markerData);

			view->CreateArray(&extraMarkerData);
			localMap_->iconDisplay.SetMember("ExtraMarkerData", extraMarkerData);

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

		auto debugWidget = DebugWidget::GetSingleton();

		if (debugWidget)
		{
			std::uint32_t usedPasses = debug::GetCurrentNumOfUsedPasses();

			debugWidget->SetRenderPasses(usedPasses);
		}
	}

	void Minimap::Advance()
	{
		if (IsVisible() && IsShown())
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

		for (RE::BSTArray<RE::NiPointer<RE::Actor>>& actorList : actorLists)
		{
			actorList.clear();
		}

		RE::BSTArray<RE::NiPointer<RE::Actor>>& enemyActors = actorLists[ExtraMarker::Type::kEnemy];
		RE::BSTArray<RE::NiPointer<RE::Actor>>& hostileActors = actorLists[ExtraMarker::Type::kHostile];
		RE::BSTArray<RE::NiPointer<RE::Actor>>& guardActors = actorLists[ExtraMarker::Type::kGuard];
		RE::BSTArray<RE::NiPointer<RE::Actor>>& deadActorList = actorLists[ExtraMarker::Type::kDead];
		
		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		for (RE::ActorHandle& highActorHandle : RE::ProcessLists::GetSingleton()->highActorHandles)
		{
			RE::NiPointer<RE::Actor> actor = highActorHandle.get();
			if (actor && RE::NiCamera::PointInFrustum(actor->GetPosition(), cameraContext->camera.get(), 1))
			{
				bool isActorEnemy = false;

				for (RE::ActorHandle& enemyActorHandle : player->GetPlayerRuntimeData().actorsToDisplayOnTheHUDArray)
				{
					if (highActorHandle == enemyActorHandle)
					{
						enemyActors.push_back(actor);
						isActorEnemy = true;
						break;
					}
				}

				if (actor->IsDead())
				{
					if (RE::TESObjectREFR_HasAnyDroppedItem(actor.get()))
					{
						deadActorList.push_back(actor);
					}
				}
				else if (!isActorEnemy && actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAggression) != 0.0F)
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

		std::size_t extraMarkerDataSize = 0;
		for (RE::BSTArray<RE::NiPointer<RE::Actor>>& actorList : actorLists)
		{
			extraMarkerDataSize += actorList.size();
		}

		extraMarkerData.SetArraySize(extraMarkerDataSize * ExtraMarker::CreateData::kStride);

		int j = 0;

		for (std::uint32_t i = 0; i < ExtraMarker::Type::kTotal; i++)
		{
			RE::BSTArray<RE::NiPointer<RE::Actor>>& actorList = actorLists[i];

			for (RE::NiPointer<RE::Actor>& actor : actorList)
			{
				extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
				extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, i);
				j += ExtraMarker::CreateData::kStride;
			}
		}

		localMap_->iconDisplay.Invoke("CreateMarkers", nullptr, nullptr, 0);
	}

	void Minimap::RefreshMarkers()
	{
		std::size_t extraMarkerDataSize = 0;
		for (RE::BSTArray<RE::NiPointer<RE::Actor>>& actorList : actorLists)
		{
			extraMarkerDataSize += actorList.size();
		}

		extraMarkerData.SetArraySize(extraMarkerDataSize * ExtraMarker::RefreshData::kStride);

		int j = 0;

		for (RE::BSTArray<RE::NiPointer<RE::Actor>>& actorList : actorLists)
		{
			for (RE::NiPointer<RE::Actor>& actor : actorList)
			{
				RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
				extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
				extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
				j += ExtraMarker::RefreshData::kStride;
			}
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
