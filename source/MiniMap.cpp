#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/B/BSShaderAccumulator.h"
#include "RE/I/ImageSpaceShaderParam.h"
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
			REL::Relocation<func_t> func{ RELOCATION_ID(75463, 0) };

			return func(this, a_effectIndex, a_swap, a_target, a_param);
        }
    };

	void BSRenderManager__sub_140D6A6D0(BSRenderManager* _this, float a_unk1, float a_unk2, float a_unk3, float a_unk4)
	{
		using func_t = decltype(&BSRenderManager__sub_140D6A6D0);
		REL::Relocation<func_t> func{ RELOCATION_ID(75463, 0) };

		func(_this, a_unk1, a_unk2, a_unk3, a_unk4);
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
    // Only called once, if we change of cell it does not get called again, and the correct image of the minimap is still rendered
	void Minimap::InitLocalMap()
	{
		if (localMap)
		{
			std::free(localMap);
		}
		localMap = static_cast<RE::LocalMapMenu*>(std::malloc(sizeof(RE::LocalMapMenu)));
		if (localMap)
		{
			localMap->Ctor();
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
	}

	bool Minimap::ProcessMessage(RE::UIMessage* a_message)
	{
		if (a_message->type == RE::UI_MESSAGE_TYPE::kScaleformEvent && !localMap)
		{
			InitLocalMap();
		}

		if (localMap)
		{
			//if (!holder->GetRuntimeData().movieView)
			{
				localMap->InitScaleform(view.get());
			}

			//localMap->GetRuntimeData().enabled = false;
			localMap->Show(true);
		}

		return false;
	}

	void Minimap::Advance(RE::HUDMenu* a_hudMenu)
	{
		if (localMap)
		{
			a_hudMenu->menuFlags.set(RE::UI_MENU_FLAGS::kRendersOffscreenTargets);

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();

			RE::LocalMapCamera* localMapCamera = localMap->localCullingProcess.GetLocalMapCamera();

			localMapCamera->defaultState->initialPosition.x = playerPos.x;
			localMapCamera->defaultState->initialPosition.y = playerPos.y;
			localMapCamera->defaultState->translation = { 0, 0, 0 };

			localMap->Advance();
		}
	}

	void Minimap::PreRender(RE::HUDMenu* a_hudMenu)
	{
		using namespace std::chrono;

		if (localMap)
		{
			RE::RenderPassCache* renderPassCache = RE::RenderPassCache::GetSingleton();

			RE::RenderPassCache::Pool& pool = renderPassCache->pools[0];

			RE::LocalMapCamera* localMapCamera = localMap->localCullingProcess.GetLocalMapCamera();
			RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->loadedAreaBound;
			RE::NiPoint3 maxExtentExtra{ 0, 0, 40000 };

			localMapCamera->minExtent = loadedAreaBound->minExtent;
			localMapCamera->maxExtent = loadedAreaBound->maxExtent + maxExtentExtra;

			static std::chrono::time_point<system_clock> last;

			std::chrono::time_point<system_clock> now = system_clock::now();

			auto diff = duration_cast<milliseconds>(now - last);

			if (diff > 40ms)
			{
				localMap->localCullingProcess.Setup();

				//localMap->localCullingProcess.RenderOffScreen();

                RenderOffscreen(&localMap->localCullingProcess);

				size_t usedPasses = 0;

				constexpr size_t passCount = 65535;
				for (size_t passIndex = 0; passIndex < passCount; ++passIndex)
				{
					const RE::BSRenderPass& pass = pool.passes[passIndex];
					if (pass.passEnum != 0)
					{
						usedPasses++;
					}
				}

				logger::debug("Total: {}", usedPasses);

				last = now;
			}
		}
	}

	void Minimap::RenderOffscreen(RE::LocalMapMenu::LocalMapCullingProcess* a_cullingProcess)
	{
		// 1. Setup culling step ///////////////////////////////////////////////////////////////////////////////////////////

		RE::ShadowSceneNode* mainShadowSceneNode = RE::ShadowSceneNode::GetMain();

		bool prevUnk219 = mainShadowSceneNode->GetRuntimeData().unk219;
		mainShadowSceneNode->GetRuntimeData().unk219 = true;

        RE::NiPointer<RE::BSShaderAccumulator>& shaderAccumulator = a_cullingProcess->GetShaderAccumulator();

		shaderAccumulator->activeShadowSceneNode = mainShadowSceneNode;

		RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& mainShadowSceneChildren = mainShadowSceneNode->GetChildren();

		RE::NiPointer<RE::NiAVObject>& objectLODRoot = mainShadowSceneChildren[3];

		bool areObjectLODsHidden = objectLODRoot->GetFlags().any(RE::NiAVObject::Flag::kHidden);

		objectLODRoot->GetFlags().reset(RE::NiAVObject::Flag::kHidden);

		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 0) };
		bool& byte_141E0DC5C = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 0) };
		bool& byte_141E0DC5D = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 0) };
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 0) };
		
		bool prevByte_1431D1D30 = byte_1431D1D30;
		bool prevByte_141E0DC5C = byte_141E0DC5C;

		byte_1431D1D30 = true;
		byte_141E0DC5C = false;
		byte_141E0DC5D = false;
		dword_1431D0D8C = 0;

        RE::BSRenderManager* renderManager = RE::BSRenderManager::GetSingleton();

		RE::BSRenderManager__sub_140D6A6D0(renderManager, 0.0F, 0.0F, 0.0F, 1.0F);

		bool& byte_141E0E350 = *REL::Relocation<bool*>{ RELOCATION_ID(513342, 0) };

		bool prevByte_141E0E350 = byte_141E0E350;

        RE::TES* tes = RE::TES::GetSingleton();
        RE::TESObjectCELL* interiorCell = tes->interiorCell;
		RE::TESWorldSpace* worldSpace = tes->worldSpace;

		bool smallWorldSpace = worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kSmallWorld);
		if (interiorCell || smallWorldSpace)
		{
			byte_141E0E350 = false;
		}
		else
		{
			byte_141E0E350 = true;
		}

		RE::LocalMapMenu::LocalMapCullingProcess::UnkData unkData{ a_cullingProcess };

		if (worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions))
		{
			unkData.unk8 = false;
		}
		else
		{
			unkData.unk8 = true;
		}

        RE::TESObjectCELL* currentCell = nullptr;
		if (interiorCell)
		{
			currentCell = interiorCell;
		}
		else if (worldSpace)
		{
			RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
			if (skyCell && skyCell->IsAttached())
            {
				if (RE::LocalMapMenu::LocalMapCullingProcess::GetTerrainData(tes->gridCells, &unkData, nullptr) == 1)
                {
					currentCell = skyCell;
				}
            }
		}

        if (currentCell)
        {
			RE::LocalMapMenu::LocalMapCullingProcess::GetCellObjectsData(&unkData, currentCell);
		}

		// 2. Culling step /////////////////////////////////////////////////////////////////////////////////////////////////

        RE::BSCullingProcess::Data& cullingData = a_cullingProcess->data;
        RE::NiPointer<RE::NiCamera> localMapCamera = a_cullingProcess->GetLocalMapCamera()->camera;

        cullingData.camera = localMapCamera;

        RE::BSPortalGraphEntry* portalGraphEntry = RE::Main__GetPortalGraphEntry(RE::Main::GetSingleton());
		
        if (portalGraphEntry)
        {
			RE::BSPortalGraph* portalGraph = portalGraphEntry->portalGraph;
            if (portalGraph)
            {
				a_cullingProcess->data.unk48 = &portalGraph->unk58;

                a_cullingProcess->data.UseForCulling(1, 0);
            }
        }

        if (mainShadowSceneChildren.capacity() > 9)
        {
			RE::NiPointer<RE::NiAVObject>& portalSharedNode = mainShadowSceneChildren[9];
            if (portalSharedNode)
            {
				cullingData.currentCulledObject = portalSharedNode;
				cullingData.UseForCulling(0, 0);
            }
        }

        if (mainShadowSceneChildren.capacity() > 8)
        {
			RE::NiPointer<RE::NiAVObject>& multiBoundNode = mainShadowSceneChildren[8];
            if (multiBoundNode)
            {
				cullingData.currentCulledObject = multiBoundNode;
				cullingData.UseForCulling(0, 0);
            }
        }

		// 3. Rendering step ///////////////////////////////////////////////////////////////////////////////////////////////

        RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

        int depthStencil = renderTargetManager->GetDepthStencil();
        renderTargetManager->sub_140D74D10(depthStencil, 0, 0);

		renderTargetManager->sub_140D74CF0(0, RE::RENDER_TARGET::kLocalMapSwap, 0, true);
		RE::NiCamera__Accumulate(a_cullingProcess->GetLocalMapCamera()->camera.get(), shaderAccumulator.get(), 0);

		// 4. Post process step (Add fog of war) ///////////////////////////////////////////////////////////////////////////

		shaderAccumulator->sub_1412CCF90(false);

		//RE::BSShaderAccumulator::SetRenderMode(19);

		RE::NiPointer<RE::NiAVObject>& fogOfWarOverlay = a_cullingProcess->GetFogOfWarOverlay();
		if (fogOfWarOverlay)
		{
			cullingData.currentCulledObject = fogOfWarOverlay;
			cullingData.UseForCulling(0, 0);
		}

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

		renderTargetManager->sub_140D74CF0(0, RE::RENDER_TARGET::kLocalMapSwap, 3, true);
		RE::NiCamera__Accumulate(a_cullingProcess->GetLocalMapCamera()->camera.get(), shaderAccumulator.get(), 0);

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

		// 5. Finish rendering and dispatch ////////////////////////////////////////////////////////////////////////////////

        renderTargetManager->sub_140D74D10(-1, 3, 0);
		RE::ImageSpaceShaderParam& imageSpaceShaderParam = a_cullingProcess->GetImageSpaceShaderParam();
		RE::BSGraphics::RenderTargetProperties& renderLocalMapSwapData = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLocalMapSwap];
		float localMapSwapWidth = renderLocalMapSwapData.width;
		float localMapSwapHeight = renderLocalMapSwapData.height;

		imageSpaceShaderParam.sub_1412D66F0(0, 1.0F / localMapSwapWidth, 1.0F / localMapSwapHeight, 0, 0);
		
        RE::TESImageSpaceManager* imageSpaceManager = RE::TESImageSpaceManager::GetSingleton();
		imageSpaceManager->sub_1412979E0(98, RE::RENDER_TARGET::kLocalMapSwap, RE::RENDER_TARGET::kLocalMap, &imageSpaceShaderParam);

		if (areObjectLODsHidden)
		{
			objectLODRoot->GetFlags().set(RE::NiAVObject::Flag::kHidden);
		}

		mainShadowSceneNode->GetRuntimeData().unk219 = prevUnk219;
		byte_141E0E350 = prevByte_141E0E350;
		byte_1431D1D30 = prevByte_1431D1D30;
		byte_141E0DC5C = prevByte_141E0DC5C;
		byte_141E0DC5D = prevByte_141E0DC5C;
		dword_1431D0D8C = 0;

        shaderAccumulator->sub_1412CCF90(false);
	}
}