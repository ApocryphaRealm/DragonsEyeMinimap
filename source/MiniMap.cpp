#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/B/BSShaderAccumulator.h"
#include "RE/I/ImageSpaceShaderParam.h"
#include "RE/L/LocalMapCamera.h"
#include "RE/R/Renderer.h"
#include "RE/R/RendererShadowState.h"
#include "RE/R/RenderPassCache.h"
#include "RE/R/RenderTargetManager.h"

namespace RE
{
	class BSPortalGraphEntry : public NiRefObject
	{
	public:

        ~BSPortalGraphEntry() override; // 00

        // members
		BSPortalGraph* portalGraph; // 10
        // ...
	};
	//static_assert(sizeof(BSPortalGraphEntry) == 0x140);

    class TESImageSpaceManager
    {
	public:
		static TESImageSpaceManager* GetSingleton()
		{
			REL::Relocation<TESImageSpaceManager**> instance{ RELOCATION_ID(527731, 0) };
			return *instance.get();
		}

        void* sub_1412979E0(std::int32_t a_effectIndex, RENDER_TARGET a_swap, RENDER_TARGET a_target, ImageSpaceShaderParam* a_param)
        {
			using func_t = decltype(&TESImageSpaceManager::sub_1412979E0);
			REL::Relocation<func_t> func{ RELOCATION_ID(99025, 0) };

			return func(this, a_effectIndex, a_swap, a_target, a_param);
        }
    };

	namespace BSGraphics
	{
		void Renderer__sub_140D6A6D0(Renderer* _this, float a_unk1, float a_unk2, float a_unk3, float a_unk4)
		{
			using func_t = decltype(&Renderer__sub_140D6A6D0);
			REL::Relocation<func_t> func{ RELOCATION_ID(75463, 0) };

			func(_this, a_unk1, a_unk2, a_unk3, a_unk4);
		}
	}

    RE::BSPortalGraphEntry* Main__GetPortalGraphEntry(Main* _this)
    {
		using func_t = decltype(&Main__GetPortalGraphEntry);
		REL::Relocation<func_t> func{ RELOCATION_ID(35607, 0) };

		return func(_this);
    }

    void* NiCamera__Accumulate(NiCamera* _this, BSShaderAccumulator* a_shaderAccumulator, std::uint32_t a_unk2)
    {
		using func_t = decltype(&NiCamera__Accumulate);
		REL::Relocation<func_t> func{ RELOCATION_ID(99789, 0) };

		return func(_this, a_shaderAccumulator, a_unk2);
    }
}

namespace DEM
{
	void Minimap::InitLocalMap()
	{
		localMap = static_cast<RE::LocalMapMenu*>(std::malloc(sizeof(RE::LocalMapMenu)));
		if (localMap)
		{
			localMap->Ctor();
			localMap_ = &localMap->GetRuntimeData();
			cullingProcess = &localMap->localCullingProcess;
			cameraContext = cullingProcess->GetLocalMapCamera();

			localMap_->movieView = view.get();

			std::string pathToRoot(DEM::Minimap::path);
			pathToRoot += ".MapClip";

			view->GetVariable(&localMap_->root, pathToRoot.c_str());

			localMap_->root.Invoke("InitMap");

			view->CreateArray(&localMap->markerData);
			localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay);
			localMap_->iconDisplay.SetMember("MarkerData", localMap->markerData);

			localMap_->enabled = true;
			localMap_->usingCursor = 0;
			localMap_->inForeground = localMap_->enabled;
			cameraContext->currentState->Begin();

			RE::GFxValue gfxEnable = localMap_->enabled;
			localMap_->root.Invoke("Show", nullptr, &gfxEnable, 1);

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
			cameraContext->SetDefaultStateInitialPosition(playerPos);
		}
	}

	std::array<RE::GFxValue, 2> Minimap::GetCurrentLocationTitle() const
	{
		std::array<RE::GFxValue, 2> locationTitle;

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		RE::TESObjectCELL* parentCell = player->parentCell;
		if (parentCell)
		{
			RE::BGSLocation* location = parentCell->GetLocation();

			if (parentCell->IsInteriorCell())
			{
				locationTitle[0] = parentCell->GetFullName();
			}
			else if (location)
			{
				locationTitle[0] = location->GetFullName();
			}
			else
			{
				RE::TESWorldSpace* worldSpace = player->GetWorldspace();
				locationTitle[0] = worldSpace->GetFullName();
			}

			if (location && location->everCleared)
			{
				locationTitle[1] = clearedStr;
			}
		}

		return locationTitle;
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
	}

	bool Minimap::ProcessMessage(RE::UIMessage* a_message)
	{
		if (!localMap)
		{
			InitLocalMap();
		}

		return false;
	}


	void Minimap::Advance()
	{
		if (localMap && localMap_->enabled)
		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

			RE::NiPoint3 playerPos = player->GetPosition();
			cameraContext->defaultState->initialPosition.x = playerPos.x;
			cameraContext->defaultState->initialPosition.y = playerPos.y;
			cameraContext->defaultState->translation = { 0, 0, 0 };

			RE::TESObjectCELL* parentCell = player->parentCell;
			if (parentCell)
			{
				float northRotation = parentCell->GetNorthRotation();
				cameraContext->SetNorthRotation(-northRotation);
				if (parentCell->IsInteriorCell())
				{					
					RE::NiPoint3 minExtent = cameraContext->minExtent;
					minExtent.x -= localMapMargin;
					minExtent.y -= localMapMargin;
					RE::NiPoint3 maxExtent = cameraContext->maxExtent;
					maxExtent.x += localMapMargin;
					maxExtent.y += localMapMargin;
					cameraContext->SetAreaBounds(maxExtent, minExtent);
				}
			}
			cameraContext->Update();

			localMap->PopulateData();
			RE::GFxValue presult;
			do
			{
				localMap_->iconDisplay.Invoke("CreateMarkers", nullptr, nullptr, 0);
				localMap_->iconDisplay.Invoke("GetCreatingMarkers", &presult, nullptr, 0);
			} while (presult.GetBool());
			localMap->RefreshMarkers();

			std::array<RE::GFxValue, 2> args = GetCurrentLocationTitle();
			localMap_->root.Invoke("SetTitle", nullptr, args);
		}
	}

	void Minimap::PreRender()
	{
		using namespace std::chrono;

		if (localMap)
		{
			RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->loadedAreaBound;
			cameraContext->SetAreaBounds(loadedAreaBound->maxExtent, loadedAreaBound->minExtent);

			std::chrono::time_point<system_clock> now = system_clock::now();

			static std::chrono::time_point<system_clock> lastUpdate;
			auto diffUpdate = duration_cast<milliseconds>(now - lastUpdate);
			if (diffUpdate > 40ms)
			{
				UpdateFogOfWar();
				RenderOffscreen();

				lastUpdate = now;
			}

			static std::chrono::time_point<system_clock> lastLog;
			auto diffLog = duration_cast<milliseconds>(now - lastLog);
			if (diffLog > 500ms)				
			{
				RE::RenderPassCache* renderPassCache = RE::RenderPassCache::GetSingleton();
				RE::RenderPassCache::Pool& pool = renderPassCache->pools[0];

				size_t usedPasses = 0;
				static constexpr size_t passCount = 65535;
				for (size_t passIndex = 0; passIndex < passCount; ++passIndex)
				{
					const RE::BSRenderPass& pass = pool.passes[passIndex];
					if (pass.passEnum != 0)
					{
						usedPasses++;
					}
				}

				//logger::debug("Total: {}", usedPasses);

				lastLog = now;
			}
		}
	}

	void Minimap::UpdateFogOfWar()
	{
		auto& fogOfWarOverlayHolder = reinterpret_cast< RE::NiPointer<RE::NiNode>&>(cullingProcess->GetFogOfWarOverlay());

		if (!fogOfWarOverlayHolder)
		{
			localMap->localCullingProcess.CreateFogOfWar();
		}
		else
		{
			// Clear last-frame fog of war overlay because I don't know how to update BSTriShapes.
			// Maybe that would improve performance.
			std::uint32_t childrenSize = fogOfWarOverlayHolder->GetChildren().size();
			for (std::uint32_t i = 0; i < childrenSize; i++)
			{
				fogOfWarOverlayHolder->DetachChildAt(i);
			}

			RE::LocalMapMenu::FogOfWar fogOfWar;

			fogOfWar.overlayHolder = fogOfWarOverlayHolder.get();

			RE::TESObjectCELL* interiorCell = RE::TES::GetSingleton()->interiorCell;

			if (interiorCell)
			{
				fogOfWar.minExtent = cameraContext->minExtent;
				fogOfWar.maxExtent = cameraContext->maxExtent;
				fogOfWar.gridDivisions = 32;
		
				cullingProcess->AttachFogOfWarOverlay(fogOfWar, interiorCell);
			}
			else
			{
				fogOfWar.gridDivisions = 16;
		
				RE::GridCellArray* gridCells = RE::TES::GetSingleton()->gridCells;
				std::uint32_t gridLength = gridCells->length;
		
				if (gridLength)
				{
					for (std::uint32_t gridX = 0; gridX < gridLength; gridX++)
					{	
						for (std::uint32_t gridY = 0; gridY < gridLength; gridY++)
						{					
							RE::TESObjectCELL* cell = gridCells->GetCell(gridX, gridY);
							if (cell && cell->IsAttached())
							{
								cullingProcess->AttachFogOfWarOverlay(fogOfWar, cell);
							}
						}
					}
				}

				RE::TESWorldSpace* worldSpace = RE::TES::GetSingleton()->worldSpace;
				if (worldSpace)
				{
					RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
					if (skyCell && skyCell->IsAttached())
					{
						cullingProcess->AttachFogOfWarOverlay(fogOfWar, skyCell);
					}
				}
			}
	
			fogOfWarOverlayHolder->local.translate.z = (cameraContext->maxExtent.z - cameraContext->minExtent.z) * 0.5;
			
			RE::NiUpdateData niUpdateData;
			niUpdateData.time = 0.0F;

			fogOfWarOverlayHolder->Update(niUpdateData);
		}
	}

	void Minimap::RenderOffscreen()
	{
		// 1. Setup culling step ///////////////////////////////////////////////////////////////////////////////////////////

		RE::ShadowSceneNode* mainShadowSceneNode = RE::ShadowSceneNode::GetMain();

        RE::NiPointer<RE::BSShaderAccumulator>& shaderAccumulator = cullingProcess->GetShaderAccumulator();

		shaderAccumulator->activeShadowSceneNode = mainShadowSceneNode;

		RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& mainShadowSceneChildren = mainShadowSceneNode->GetChildren();

        RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();

		RE::BSGraphics::Renderer__sub_140D6A6D0(renderer, 0.0F, 0.0F, 0.0F, 1.0F);

        RE::TES* tes = RE::TES::GetSingleton();
		RE::TESWorldSpace* worldSpace = tes->worldSpace;

		RE::LocalMapMenu::LocalMapCullingProcess::UnkData unkData{ cullingProcess };
		
		bool isWaterRenderingEnabled = enableWaterRendering;
		enableWaterRendering = false;

		if (worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions))
		{
			unkData.unk8 = false;
		}
		else 
		{
			unkData.unk8 = true;
		}

        RE::TESObjectCELL* currentCell = nullptr;

		RE::TESObjectCELL* interiorCell = tes->interiorCell;

		// 2. Culling step /////////////////////////////////////////////////////////////////////////////////////////////////

		if (interiorCell)
		{
			currentCell = interiorCell;
		}
		else if (worldSpace)
		{
			RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
			if (skyCell && skyCell->IsAttached())
            {
				if (cullingProcess->CullExteriorGround(tes->gridCells, unkData, nullptr) == 1)
				{
					currentCell = skyCell;
				}
            }
		}

        if (currentCell)
        {
			cullingProcess->CullCellObjects(unkData, currentCell);
		}

        RE::BSCullingDelegate& cullingDelegate = cullingProcess->culler;
		RE::NiPointer<RE::NiCamera> camera = cameraContext->camera;

        cullingDelegate.camera = camera;

        RE::BSPortalGraphEntry* portalGraphEntry = RE::Main__GetPortalGraphEntry(RE::Main::GetSingleton());
		
        if (portalGraphEntry)
        {
			RE::BSPortalGraph* portalGraph = portalGraphEntry->portalGraph;
            if (portalGraph)
            {
				cullingDelegate.unk48 = &portalGraph->unk58;
				cullingDelegate.Cull(1, 0);
            }
        }

        if (mainShadowSceneChildren.capacity() > 9)
        {
			RE::NiPointer<RE::NiAVObject>& portalSharedNode = mainShadowSceneChildren[9];
			cullingDelegate.currentCulledObject = portalSharedNode;
        }
		else
		{
			cullingDelegate.currentCulledObject = nullptr;
		}
		cullingDelegate.Cull(0, 0);

        if (mainShadowSceneChildren.capacity() > 8)
        {
			RE::NiPointer<RE::NiAVObject>& multiBoundNode = mainShadowSceneChildren[8];
			cullingDelegate.currentCulledObject = multiBoundNode;
        }
		else
		{
			cullingDelegate.currentCulledObject = nullptr; 
		}
		cullingDelegate.Cull(0, 0);

		// 3. Rendering step ///////////////////////////////////////////////////////////////////////////////////////////////

        RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

        int depthStencil = renderTargetManager->GetDepthStencil();
        renderTargetManager->sub_140D74D10(depthStencil, 0, 0, false);

		RE::BSGraphics::SetRenderTargetMode worldRenderTargetMode = isFogOfWarEnabled ? RE::BSGraphics::SetRenderTargetMode::kClear : RE::BSGraphics::SetRenderTargetMode::kRestore;

		renderTargetManager->SetModeForRenderTarget(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, worldRenderTargetMode, true);
		RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

		// 4. Post process step (Add fog of war) ///////////////////////////////////////////////////////////////////////////

		if (isFogOfWarEnabled)
		{
			shaderAccumulator->sub_1412CCF90(false);

			RE::BSShaderAccumulator::SetRenderMode(19);

			RE::NiPointer<RE::NiAVObject> fogOfWarOverlayHolder = cullingProcess->GetFogOfWarOverlay();
			cullingDelegate.currentCulledObject = fogOfWarOverlayHolder;
			cullingDelegate.Cull(0, 0);

			RE::BSGraphics::RendererShadowState* rendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

			if (rendererShadowState->alphaBlendWriteMode != 8)
			{
				rendererShadowState->alphaBlendWriteMode = 8;
				rendererShadowState->stateUpdateFlags |= 0x80;
			}

			if (rendererShadowState->depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kDisabled)
			{
				rendererShadowState->depthStencilDepthMode = RE::BSGraphics::DepthStencilDepthMode::kDisabled;
				if (rendererShadowState->depthStencilUnknown)
				{
					rendererShadowState->stateUpdateFlags |= 4;
				}
				else
				{
					rendererShadowState->stateUpdateFlags &= ~4;
				}
			}

			renderTargetManager->SetModeForRenderTarget(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::kRestore, true);
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

			if (rendererShadowState->depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kTestWrite)
			{
				rendererShadowState->depthStencilDepthMode = RE::BSGraphics::DepthStencilDepthMode::kTestWrite;
				if (rendererShadowState->depthStencilUnknown != 3)
				{
					rendererShadowState->stateUpdateFlags |= 4;
				}
				else
				{
					rendererShadowState->stateUpdateFlags &= ~4;
				}
			}

			if (rendererShadowState->alphaBlendWriteMode != 1)
			{
				rendererShadowState->alphaBlendWriteMode = 1;
				rendererShadowState->stateUpdateFlags |= 0x80;
			}
		}
		// 5. Finish rendering and dispatch ////////////////////////////////////////////////////////////////////////////////

        renderTargetManager->sub_140D74D10(-1, 3, 0, false);
		RE::ImageSpaceShaderParam& imageSpaceShaderParam = cullingProcess->GetImageSpaceShaderParam();
		RE::BSGraphics::RenderTargetProperties& renderLocalMapSwapData = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];
		float localMapSwapWidth = renderLocalMapSwapData.width;
		float localMapSwapHeight = renderLocalMapSwapData.height;

		imageSpaceShaderParam.sub_1412D66F0(0, 1.0 / localMapSwapWidth, 1.0 / localMapSwapHeight, 0.0, 0.0);
		
        RE::TESImageSpaceManager* imageSpaceManager = RE::TESImageSpaceManager::GetSingleton();
		imageSpaceManager->sub_1412979E0(98, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::RENDER_TARGET::kLOCAL_MAP, &imageSpaceShaderParam);

		enableWaterRendering = isWaterRenderingEnabled;

        shaderAccumulator->sub_1412CCF90(false);
	}
}
