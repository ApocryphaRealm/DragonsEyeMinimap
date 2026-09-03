#include "Minimap.h"

#include "RE/B/BSTimer.h"
#include "RE/I/ImageSpaceManager.h"
#include "RE/I/ImageSpaceShaderParam.h"
#include "RE/L/LocalMapCamera.h"
#include "RE/R/Renderer.h"
#include "RE/S/ShaderAccumulator.h"
#include "RE/S/ShadowState.h"
#include "RE/R/RenderPassCache.h"
#include "RE/R/RenderTargetManager.h"

namespace RE
{
	class BSPortalGraphEntry : public NiRefObject
	{
	public:
		~BSPortalGraphEntry() override;	 // 00

		// members
		BSPortalGraph* portalGraph;	 // 10
									 // ...
	};
	//static_assert(sizeof(BSPortalGraphEntry) == 0x140);

	RE::BSPortalGraphEntry* Main__GetPortalGraphEntry(Main* a_this)
	{
		using func_t = decltype(&Main__GetPortalGraphEntry);
		REL::Relocation<func_t> func{ RELOCATION_ID(35607, 36617) };

		return func(a_this);
	}

	void* NiCamera__Accumulate(NiCamera* a_this, BSGraphics::BSShaderAccumulator* a_shaderAccumulator, std::uint32_t a_unk2)
	{
		using func_t = decltype(&NiCamera__Accumulate);
		REL::Relocation<func_t> func{ RELOCATION_ID(99789, 106436) };

		return func(a_this, a_shaderAccumulator, a_unk2);
	}
}

namespace DEM
{
	void Minimap::UpdateFogOfWar()
	{
		auto& fogOfWarOverlayHolder = reinterpret_cast< RE::NiPointer<RE::NiNode>&>(cullingProcess->GetFogOfWarOverlay());

		if (!fogOfWarOverlayHolder)
		{
			logger::debug("Fog of war overlay not created yet; creating it");
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
			bool skyCellAttached = false;

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

				RE::TESWorldSpace* worldSpace = RE::TES::GetSingleton()->GetRuntimeData2().worldSpace;
				if (worldSpace)
				{
					RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
					if (skyCell && skyCell->IsAttached())
					{
						cullingProcess->AttachFogOfWarOverlay(fogOfWar, skyCell);
						skyCellAttached = true;
					}
				}
			}

			// Runs every frame while fog of war is enabled - only log when the branch taken actually changes.
			static bool s_loggedFogOfWarState = false;
			static bool s_lastFogOfWarInterior = false;
			static bool s_lastFogOfWarSkyAttached = false;
			bool isFogOfWarInterior = interiorCell != nullptr;
			if (!s_loggedFogOfWarState || isFogOfWarInterior != s_lastFogOfWarInterior ||
				(!isFogOfWarInterior && skyCellAttached != s_lastFogOfWarSkyAttached))
			{
				if (isFogOfWarInterior)
				{
					logger::debug("Fog of war updating for interior cell {:X}", interiorCell->GetFormID());
				}
				else
				{
					logger::debug("Fog of war updating for exterior grid; sky cell overlay {}", skyCellAttached ? "attached" : "not attached");
				}

				s_loggedFogOfWarState = true;
				s_lastFogOfWarInterior = isFogOfWarInterior;
				s_lastFogOfWarSkyAttached = skyCellAttached;
			}

			fogOfWarOverlayHolder->local.translate.z = (cameraContext->maxExtent.z - cameraContext->minExtent.z) * 0.5;
			
			RE::NiUpdateData niUpdateData;
			niUpdateData.time = 0.0F;

			fogOfWarOverlayHolder->Update(niUpdateData);
		}
	}

	bool Minimap::ShouldRedrawWorld()
	{
		using clock = std::chrono::steady_clock;

		const clock::time_point now = clock::now();

		const RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		const RE::TES* tes = RE::TES::GetSingleton();

		const void* worldSpace = tes ? static_cast<const void*>(tes->GetRuntimeData2().worldSpace) : nullptr;
		const bool interior = tes && tes->interiorCell != nullptr;
		const RE::NiPoint3 playerPos = player ? player->GetPosition() : RE::NiPoint3{};

		// A load, not ordinary movement - see the members these compare against for why the
		// parent cell is deliberately not one of these tests.
		const char* reason = nullptr;

		if (worldSpace != lastSeenWorldSpace)
		{
			reason = "worldspace changed";
		}
		else if (interior != lastSeenInterior)
		{
			reason = "moved between an interior and an exterior";
		}
		else if (hasLastPlayerPos && lastPlayerPos.GetDistance(playerPos) > kTeleportDistance)
		{
			reason = "the player was moved further than one frame of travel";
		}

		lastSeenWorldSpace = worldSpace;
		lastSeenInterior = interior;
		lastPlayerPos = playerPos;
		hasLastPlayerPos = true;

		if (reason)
		{
			worldChangedAt = now;

			logger::debug("World changed ({}); holding the map redraw for {} ms while it settles",
				reason, settings::rendering::settleMs);
		}

		if (settings::rendering::skipWhileWorldSettles)
		{
			// A loading screen is the clearest "not steady" signal there is, and the settle
			// window has to start when it ENDS, not when the change behind it was noticed.
			RE::UI* ui = RE::UI::GetSingleton();
			if (ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
			{
				worldChangedAt = now;
				return false;
			}

			const auto sinceChange = std::chrono::duration_cast<std::chrono::milliseconds>(now - worldChangedAt).count();
			if (sinceChange < settings::rendering::settleMs)
			{
				return false;
			}
		}

		if (settings::rendering::redrawIntervalMs > 0)
		{
			const auto sinceRedraw = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWorldRedrawAt).count();
			if (sinceRedraw < settings::rendering::redrawIntervalMs)
			{
				return false;
			}
		}

		lastWorldRedrawAt = now;
		return true;
	}

	void Minimap::RenderOffScreen()
	{
		LMU::PixelShaderProperty::Shape prevShaderShape;
		LMU::PixelShaderProperty::Style shaderStyle;

		// Local Map Upgrade hands these over in its kPixelShaderPropertiesHook message, which it
		// dispatches from its own kDataLoaded handler. SKSE delivers kDataLoaded in load order, so
		// whether they have arrived at any given earlier moment depends on which plugin loaded
		// first - it is not something to assert on. They are reliably present by the time anything
		// is actually rendered, which is here. Checked rather than assumed all the same, per
		// CLAUDE.md rule 14 (debug logging and null checks together) and rule 17 (retry a lookup
		// that can fail because something is not ready yet - a not-ready pointer is not an error,
		// it just means not yet).
		if (!GetPixelShaderProperties || !SetPixelShaderProperties)
		{
			// Per-frame path, so this logs once rather than every frame.
			static bool warnedMissingShaderHooks = false;
			if (!warnedMissingShaderHooks)
			{
				logger::warn("RenderOffScreen: Local Map Upgrade's pixel-shader hooks are not available yet; drawing without the shape swap this frame");
				warnedMissingShaderHooks = true;
			}

			return;
		}

		GetPixelShaderProperties(prevShaderShape, shaderStyle);
		SetPixelShaderProperties(shape, shaderStyle);

		// Runs every frame - only log the shader shape swap when it actually differs from last frame.
		static bool s_loggedShapeSwap = false;
		static LMU::PixelShaderProperty::Shape s_lastLoggedPrevShape{};
		static LMU::PixelShaderProperty::Shape s_lastLoggedTargetShape{};
		if (!s_loggedShapeSwap || prevShaderShape != s_lastLoggedPrevShape || shape != s_lastLoggedTargetShape)
		{
			logger::debug("Off-screen render: swapping pixel shader shape from {} to {} to draw the minimap, restoring after",
				prevShaderShape == LMU::PixelShaderProperty::Shape::kRound ? "round" : "squared",
				shape == LMU::PixelShaderProperty::Shape::kRound ? "round" : "squared");
			s_loggedShapeSwap = true;
			s_lastLoggedPrevShape = prevShaderShape;
			s_lastLoggedTargetShape = shape;
		}

		// 1. Setup culling step ///////////////////////////////////////////////////////////////////////////////////////////

		useMapBrightnessAndContrastBoost = true;

		RE::ShadowSceneNode* mainShadowSceneNode = RE::ShadowSceneNode::GetMain();

        RE::NiPointer<RE::BSGraphics::BSShaderAccumulator>& shaderAccumulator = cullingProcess->GetShaderAccumulator();

		shaderAccumulator->GetRuntimeData().activeShadowSceneNode = mainShadowSceneNode;

		RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& mainShadowSceneChildren = mainShadowSceneNode->GetChildren();

		bool isLightUpdateDisabled = mainShadowSceneNode->GetRuntimeData().disableLightUpdate;
		mainShadowSceneNode->GetRuntimeData().disableLightUpdate = true;

		RE::NiPointer<RE::NiAVObject>& objectLODRoot = mainShadowSceneChildren[3];
		bool areLODsHidden = objectLODRoot->GetFlags().any(RE::NiAVObject::Flag::kHidden);
		objectLODRoot->GetFlags().reset(RE::NiAVObject::Flag::kHidden);

		bool isByte_1431D1D30 = byte_1431D1D30;
		bool isNodeFadeEnabled = nodeFadeEnabled;
		byte_1431D1D30 = true;
		nodeDrawFadeEnabled = nodeFadeEnabled = false;
		dword_1431D0D8C = 0;

		RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
		renderer->SetClearColor(0.0F, 0.0F, 0.0F, 1.0F);

        RE::TES* tes = RE::TES::GetSingleton();
		RE::TESWorldSpace* worldSpace = tes->GetRuntimeData2().worldSpace;

		RE::LocalMapMenu::LocalMapCullingProcess::UnkData unkData{ cullingProcess };
		
		if (worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions))
		{
			unkData.unk8 = false;
		}
		else
		{
			unkData.unk8 = true;
		}

		// Runs every frame - only log when unk8 actually changes (e.g. on a worldspace change).
		static bool s_loggedUnk8 = false;
		static bool s_lastUnk8 = false;
		if (!s_loggedUnk8 || unkData.unk8 != s_lastUnk8)
		{
			logger::debug("Worldspace has fixed dimensions: {}; culling unk8 set to {}",
				worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions), unkData.unk8);
			s_loggedUnk8 = true;
			s_lastUnk8 = unkData.unk8;
		}

		// 2. Culling step /////////////////////////////////////////////////////////////////////////////////////////////////

		RE::TESObjectCELL* currentCell = nullptr;

		if (RE::TESObjectCELL* interiorCell = tes->interiorCell)
		{
			currentCell = interiorCell;
		}
		else if (worldSpace)
		{
			RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
			if (skyCell && skyCell->IsAttached())
            {
				currentCell = skyCell;

				CullTerrain(tes->gridCells, unkData, nullptr);
            }
		}

		// Runs every frame - only log when which cell we're culling against actually changes.
		static bool s_loggedCellCategory = false;
		static int s_lastCellCategory = -1;
		int cellCategory = currentCell ? (currentCell == tes->interiorCell ? 0 : 1) : 2;
		if (!s_loggedCellCategory || cellCategory != s_lastCellCategory)
		{
			switch (cellCategory)
			{
			case 0:
				logger::debug("Off-screen render culling interior cell {:X}", currentCell->GetFormID());
				break;
			case 1:
				logger::debug("Off-screen render culling exterior sky cell {:X}", currentCell->GetFormID());
				break;
			default:
				logger::debug("Off-screen render has no resolvable cell to cull (no interior cell, no attached sky cell)");
				break;
			}
			s_loggedCellCategory = true;
			s_lastCellCategory = cellCategory;
		}

        if (currentCell)
        {
			cullingProcess->CullCellObjects(unkData, currentCell);
		}

        RE::CullJobDescriptor& cullJobDesc = cullingProcess->cullJobDesc;
		RE::NiPointer<RE::NiCamera> camera = cameraContext->camera;

        cullJobDesc.camera = camera;

        RE::BSPortalGraphEntry* portalGraphEntry = RE::Main__GetPortalGraphEntry(RE::Main::GetSingleton());
		
		// Edge-triggered: this runs every frame, but a missing portal graph would mean degraded
		// culling every frame it stays missing, so only log the transition in and out of that state.
		static bool s_portalCullingDegraded = false;

        if (portalGraphEntry)
        {
			RE::BSPortalGraph* portalGraph = portalGraphEntry->portalGraph;
            if (portalGraph)
            {
				cullJobDesc.cullingObjects = reinterpret_cast<RE::BSTArray<RE::NiPointer<RE::NiAVObject>>*>(&portalGraph->unk58);
				cullJobDesc.Cull(1, 0);

				if (s_portalCullingDegraded)
				{
					logger::debug("Portal graph culling data available again");
					s_portalCullingDegraded = false;
				}
            }
			else if (!s_portalCullingDegraded)
			{
				logger::warn("Portal graph entry has no portal graph; skipping portal-object culling");
				s_portalCullingDegraded = true;
			}
        }
		else if (!s_portalCullingDegraded)
		{
			logger::warn("Could not get the portal graph entry; skipping portal-object culling");
			s_portalCullingDegraded = true;
		}

        if (mainShadowSceneChildren.capacity() > 9)
        {
			RE::NiPointer<RE::NiAVObject>& portalSharedNode = mainShadowSceneChildren[9];
			cullJobDesc.scene = portalSharedNode;
        }
		else
		{
			cullJobDesc.scene = nullptr;

			// One-shot: the shadow scene's child count shouldn't shrink back once the engine has
			// initialized it, so don't keep re-logging this every frame if it stays missing.
			static bool s_loggedPortalSharedMissing = false;
			if (!s_loggedPortalSharedMissing)
			{
				logger::warn("Shadow scene has no portal-shared node (children capacity {}); skipping its culling",
					mainShadowSceneChildren.capacity());
				s_loggedPortalSharedMissing = true;
			}
		}
		cullJobDesc.Cull(0, 0);

        if (mainShadowSceneChildren.capacity() > 8)
        {
			RE::NiPointer<RE::NiAVObject>& multiBoundNode = mainShadowSceneChildren[8];
			cullJobDesc.scene = multiBoundNode;
        }
		else
		{
			cullJobDesc.scene = nullptr;

			static bool s_loggedMultiBoundMissing = false;
			if (!s_loggedMultiBoundMissing)
			{
				logger::warn("Shadow scene has no multibound node (children capacity {}); skipping its culling",
					mainShadowSceneChildren.capacity());
				s_loggedMultiBoundMissing = true;
			}
		}
		cullJobDesc.Cull(0, 0);

		// 3. Rendering step ///////////////////////////////////////////////////////////////////////////////////////////////

        RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

        int depthStencil = renderTargetManager->GetDepthStencil();
		renderTargetManager->SetupDepthStencilAt(depthStencil, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, 0, false);
		renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, true);

		RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

		// 4. Post process step (Add fog of war) ///////////////////////////////////////////////////////////////////////////

		// Runs every frame - only log when the fog-of-war compositing step turns on or off.
		static bool s_loggedFogCompositeState = false;
		static bool s_lastFogCompositeEnabled = false;
		if (!s_loggedFogCompositeState || isFogOfWarEnabled != s_lastFogCompositeEnabled)
		{
			logger::debug("Fog of war compositing {}", isFogOfWarEnabled ? "enabled for this frame" : "disabled; skipping compositing step");
			s_loggedFogCompositeState = true;
			s_lastFogCompositeEnabled = isFogOfWarEnabled;
		}

		if (isFogOfWarEnabled)
		{
			shaderAccumulator->ClearActiveRenderPasses(false);

			RE::BSGraphics::BSShaderAccumulator::SetRenderMode(19);

			RE::NiPointer<RE::NiAVObject> fogOfWarOverlayHolder = cullingProcess->GetFogOfWarOverlay();
			cullJobDesc.scene = fogOfWarOverlayHolder;
			cullJobDesc.Cull(0, 0);

			RE::BSGraphics::RendererShadowState* rendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

			rendererShadowState->SetAlphaBlendWriteMode(8);
			rendererShadowState->SetDepthStencilDepthMode(RE::BSGraphics::DepthStencilDepthMode::kDisabled);

			renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, true);
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

			rendererShadowState->SetDepthStencilDepthMode(RE::BSGraphics::DepthStencilDepthMode::kTestWrite);
			rendererShadowState->SetAlphaBlendWriteMode(1);
		}

		// 5. Finish rendering and dispatch ////////////////////////////////////////////////////////////////////////////////

        renderTargetManager->SetupDepthStencilAt(-1, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, 0, false);

		RE::ImageSpaceShaderParam& imageSpaceShaderParam = cullingProcess->GetImageSpaceShaderParam();

		RE::BSGraphics::RenderTargetProperties& renderLocalMapSwapData = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];

		float localMapSwapWidth = renderLocalMapSwapData.width;
		float localMapSwapHeight = renderLocalMapSwapData.height;

		imageSpaceShaderParam.SetupPixelConstantGroup(0, 1.0 / localMapSwapWidth, 1.0 / localMapSwapHeight, 0.0, 0.0);
		
        RE::ImageSpaceManager* imageSpaceManager = RE::ImageSpaceManager::GetSingleton();

		imageSpaceManager->CopyWithImageSpaceEffect(RE::ImageSpaceManager::ImageSpaceEffectEnum::ISLocalMap,
													RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::RENDER_TARGET::kLOCAL_MAP,
													&imageSpaceShaderParam);

		if (areLODsHidden)
		{
			objectLODRoot->GetFlags().set(RE::NiAVObject::Flag::kHidden);
		}

		mainShadowSceneNode->GetRuntimeData().disableLightUpdate = isLightUpdateDisabled;
		byte_1431D1D30 = isByte_1431D1D30;
		nodeDrawFadeEnabled = nodeFadeEnabled = isNodeFadeEnabled;
		dword_1431D0D8C = 0;

        shaderAccumulator->ClearActiveRenderPasses(false);

		useMapBrightnessAndContrastBoost = false;

		SetPixelShaderProperties(prevShaderShape, shaderStyle);
	}

	// Terrain render passes can be allocated multiple times but only cleared once per frame.
	// I don't understand why only terrain render passes work this way, but once Draw is called
	// before entering my code, make sure we clear them so there is no memory leakage.
	void Minimap::ClearTerrainRenderPasses(RE::NiPointer<RE::NiAVObject>& a_object)
	{
		RE::NiNode* node = a_object->AsNode();

		if (!node)
		{
			return;
		}

		for (RE::NiPointer<RE::NiAVObject>& object : node->children) if (object)
		{
			if (object->flags.any(RE::NiAVObject::Flag::kRenderUse))
			{
				if (RE::BSGeometry* geometry = object->AsGeometry())
				{
					auto shaderProp = static_cast<RE::BSShaderProperty*>(geometry->properties[RE::BSGeometry::States::kEffect].get());
					if (shaderProp)
					{
						shaderProp->DoClearRenderPasses();
					}
				}
			}

			ClearTerrainRenderPasses(object);
		}
	};

	void Minimap::CullTerrain(const RE::GridCellArray* a_gridCells, RE::LocalMapMenu::LocalMapCullingProcess::UnkData& a_unkData,
							  const RE::TESObjectCELL* a_cell)
	{
		RE::CullJobDescriptor& cullJobDesc = a_unkData.ptr->cullJobDesc;

		// Called once per frame while in an exterior worldspace with an attached sky cell - only
		// log when the grid size or the unk8-gated index-2 pass actually changes, not every call.
		static bool s_loggedTerrainCullParams = false;
		static std::uint32_t s_lastGridLength = 0;
		static bool s_lastTerrainUnk8 = false;
		if (!s_loggedTerrainCullParams || a_gridCells->length != s_lastGridLength || a_unkData.unk8 != s_lastTerrainUnk8)
		{
			logger::debug("Culling terrain across a {0}x{0} grid (index-2 render passes {1})",
				a_gridCells->length, a_unkData.unk8 ? "included" : "excluded");
			s_loggedTerrainCullParams = true;
			s_lastGridLength = a_gridCells->length;
			s_lastTerrainUnk8 = a_unkData.unk8;
		}

		for (int gridCellX = 0; gridCellX < a_gridCells->length; gridCellX++)
		{
			for (int gridCellY = 0; gridCellY < a_gridCells->length; gridCellY++)
			{
				RE::TESObjectCELL* cell = a_gridCells->GetCell(gridCellX, gridCellY);
				if (cell && cell->IsAttached())
				{
					for (int sceneIndex : { 2, 3, 6, 7 }) if (sceneIndex != 2 || a_unkData.unk8)
					{
						if (RE::NiPointer<RE::NiNode> cell3D = cell->GetRuntimeData().loadedData->cell3D)
						{
							RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& cellScenes = cell3D->GetChildren();

							if (sceneIndex < cellScenes.size())
							{
								if (RE::NiPointer<RE::NiAVObject> scene = cellScenes[sceneIndex])
								{
									if (sceneIndex == 2)
									{
										ClearTerrainRenderPasses(scene);
									}

									bool isCellSceneHidden = scene->flags.any(RE::NiAVObject::Flag::kHidden);
									scene->flags.reset(RE::NiAVObject::Flag::kHidden);

									cullJobDesc.scene = scene;

									cullJobDesc.Cull(0, 0);

									if (isCellSceneHidden)
									{
										scene->flags.set(RE::NiAVObject::Flag::kHidden);
									}
								}
							}
						}
					}
				}
			}
		}

		cullJobDesc.scene = nullptr;
	}
}