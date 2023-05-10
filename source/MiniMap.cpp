#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/R/RenderPassCache.h"

namespace DEM
{
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

			//localMap->localCullingProcess.Dtor();
			//localMap->localCullingProcess.Ctor();

			RE::LocalMapCamera* localMapCamera = localMap->localCullingProcess.GetLocalMapCamera();
			RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->loadedAreaBound;
			RE::NiPoint3 maxExtentExtra{ 0, 0, 40000 };

			localMapCamera->minExtent = loadedAreaBound->minExtent;
			localMapCamera->maxExtent = loadedAreaBound->maxExtent + maxExtentExtra;

			static bool set;

			if (!set)
			{
				localMap->localCullingProcess.Setup();

				set = true;
			}

			static std::chrono::time_point<system_clock> last;

			std::chrono::time_point<system_clock> now = system_clock::now();

			auto diff = duration_cast<milliseconds>(now - last);

			if (diff > 40ms)
			{
				static RE::NiPoint3 lastPosition;
				
				RE::NiPoint3 position = RE::PlayerCharacter::GetSingleton()->GetPosition();

				if (position.GetSquaredDistance(lastPosition) > 100)
				{
					localMap->localCullingProcess.RenderOffScreen();

					lastPosition = position;
				}

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

	void Minimap::RenderOffscreen(RE::LocalMapMenu::LocalMapCullingProcess& a_cullingProcess)
	{
		shadowSceneNode = ShadowSceneNode::pArray[0];
		unk219 = ShadowSceneNode::pArray[0]->unk219;
		ShadowSceneNode::pArray[0]->unk219 = 1;
		this->shaderAccumulator->activeShadowSceneNode = shadowSceneNode;
		v4 = (NiNode*)*((_QWORD*)shadowSceneNode->children.data + 3);
		flags = v4->flags;
		v4->flags &= ~NiAVObject_Flag_Hidden;
		v6 = flags & NiAVObject_Flag_Hidden;
		v7 = byte_1431D1D30;
		v8 = byte_141E0DC5C;
		byte_1431D1D30 = 1;
		byte_141E0DC5C = 0;
		byte_141E0DC5D = 0;
		dword_1431D0D8C = 0;
		sub_1403DE280((__int64)BSTreeManager::pSingleton, 1u);
		sub_140D6A6D0(&BSRenderManager::singleton, 0.0, 0.0, 0.0, 1.0);
		tes = TES::pSingleton;
		v32 = byte_141E0E350;
		if (!TES::pSingleton->interiorCell) {
			if (!TES::GetWorldSpace(TES::pSingleton)) {
				goto LABEL_27;
			}
			WorldSpace = TES::GetWorldSpace(TES::pSingleton);
			if (!TESWorldSpace::IsSmallWorld(WorldSpace)) {
LABEL_27:
				tes = TES::pSingleton;
				byte_141E0E350 = 1;
				goto LABEL_6;
			}
			tes = TES::pSingleton;
		}
		byte_141E0E350 = 0;
LABEL_6:
		if (!TES::GetWorldSpace(tes)) {
			goto LABEL_8;
		}
		v31 = 0;
		if ((TES::GetWorldSpace(TES::pSingleton)->flags & TESWorldSpace_Flag_FixedDimensions) == 0) {
LABEL_8:
			v31 = 1;
		}
		v11 = TES::pSingleton;
		currentCell = TES::pSingleton->interiorCell;
		if (currentCell) {
			goto LABEL_15;
		}
		if (sub_1401F6900(TES::pSingleton->gridCells, &this, 0i64) == 1) {
			v13 = v11->worldSpace;
			if (v13) {
				skyCell = TESWorldSpace::GetSkyCell(v13);
				if (skyCell) {
					if (skyCell->cellState == TESObjectCELL_State_Attached) {
						currentCell = skyCell;
LABEL_15:
						sub_1401F6CA0(&this, currentCell);
					}
				}
			}
		}
		niCamera = this->camera.camera;
		dataNiCamera = this->cullingData.camera;
		if (dataNiCamera != niCamera) {
			if (niCamera) {
				_InterlockedIncrement(&niCamera->refCount);
			}
			this->cullingData.camera = niCamera;
			if (dataNiCamera && _InterlockedExchangeAdd(&dataNiCamera->refCount, -1u) == 1) {
				dataNiCamera->DeleteThis(dataNiCamera);
			}
		}
		v17 = sub_1405B6450();
		if (v17) {
			portalGraph = v17->portalGraph;
			if (portalGraph) {
				this->cullingData.unk48 = &portalGraph->unk58;
				sub_1412D6BB0(&this->cullingData, 1, 0);
			}
		}
		if (ShadowSceneNode::pArray[0]->children._freeIdx > 9u) {
			v19 = (NiNode*)*((_QWORD*)ShadowSceneNode::pArray[0]->children.data + 9);
		} else {
			v19 = 0i64;
		}
		unk40 = this->cullingData.unk40;
		if (unk40 != v19) {
			if (v19) {
				_InterlockedIncrement(&v19->refCount);
			}
			this->cullingData.unk40 = v19;
			if (unk40 && _InterlockedExchangeAdd(&unk40->refCount, -1u) == 1) {
				unk40->DeleteThis(unk40);
			}
		}
		sub_1412D6BB0(&this->cullingData, 0, 0);
		if (ShadowSceneNode::pArray[0]->children._freeIdx > 8u) {
			v21 = (NiNode*)*((_QWORD*)ShadowSceneNode::pArray[0]->children.data + 8);
		} else {
			v21 = 0i64;
		}
		v22 = this->cullingData.unk40;
		if (v22 != v21) {
			if (v21) {
				_InterlockedIncrement(&v21->refCount);
			}
			this->cullingData.unk40 = v21;
			if (v22 && _InterlockedExchangeAdd(&v22->refCount, -1u) == 1) {
				v22->DeleteThis(v22);
			}
		}
		sub_1412D6BB0(&this->cullingData, 0, 0);
		v23 = sub_140D74E50();
		BSGraphics::RenderTargetManager::sub_140D74D10(&BSGraphics::RenderTargetManager::singleton, v23, 0, 0);
		sub_140D74CF0(&BSGraphics::RenderTargetManager::singleton, 0, BSGraphics_RenderTarget_local_map_swap, 0);
		NiCamera::CumulateShader(this->camera.camera, this->shaderAccumulator, 0);

		if (isFogOfWarEnabled) {
			sub_1412CCF90(this->shaderAccumulator, 0);
			sub_141295E90(19u);
			fogOfWarOverlay = this->fogOfWarOverlay;
			v25 = this->cullingData.unk40;
			if (v25 != fogOfWarOverlay) {
				if (fogOfWarOverlay) {
					_InterlockedIncrement(&fogOfWarOverlay->refCount);
				}
				this->cullingData.unk40 = fogOfWarOverlay;
				if (v25 && _InterlockedExchangeAdd(&v25->refCount, -1u) == 1) {
					v25->DeleteThis(v25);
				}
			}
			sub_1412D6BB0(&this->cullingData, 0, 0);
			stateUpdateFlags = BSGraphics::RendererShadowState::instance.stateUpdateFlags;
			if (BSGraphics::RendererShadowState::instance.alphaBlendWriteMode != 8) {
				stateUpdateFlags = BSGraphics::RendererShadowState::instance.stateUpdateFlags | 0x80;
				BSGraphics::RendererShadowState::instance.alphaBlendWriteMode = 8;
				BSGraphics::RendererShadowState::instance.stateUpdateFlags |= 0x80u;
			}
			if (BSGraphics::RendererShadowState::instance.depthStencilDepthMode) {
				BSGraphics::RendererShadowState::instance.depthStencilDepthMode = BSGraphics_DepthStencilDepthMode_Disabled;
				v27 = stateUpdateFlags | 4;
				v28 = stateUpdateFlags & ~4u;
				if (BSGraphics::RendererShadowState::instance.depthStencilUnknown) {
					v28 = v27;
				}
				BSGraphics::RendererShadowState::instance.stateUpdateFlags = v28;
			}

			sub_140D74CF0(&BSGraphics::RenderTargetManager::singleton, 0, BSGraphics_RenderTarget_local_map_swap, 3);

			NiCamera::CumulateShader(this->camera.camera, this->shaderAccumulator, 0);

			v29 = BSGraphics::RendererShadowState::instance.stateUpdateFlags;
			if (BSGraphics::RendererShadowState::instance.depthStencilDepthMode != BSGraphics_DepthStencilDepthMode_TestWrite) {
				BSGraphics::RendererShadowState::instance.depthStencilDepthMode = BSGraphics_DepthStencilDepthMode_TestWrite;
				v29 = BSGraphics::RendererShadowState::instance.stateUpdateFlags & ~4u;
				if (BSGraphics::RendererShadowState::instance.depthStencilUnknown != 3) {
					v29 = BSGraphics::RendererShadowState::instance.stateUpdateFlags | 4;
				}
				BSGraphics::RendererShadowState::instance.stateUpdateFlags = v29;
			}
			if (BSGraphics::RendererShadowState::instance.alphaBlendWriteMode != 1) {
				BSGraphics::RendererShadowState::instance.alphaBlendWriteMode = 1;
				BSGraphics::RendererShadowState::instance.stateUpdateFlags = v29 | 0x80;
			}
		}

		BSGraphics::RenderTargetManager::sub_140D74D10(&BSGraphics::RenderTargetManager::singleton, -1, 3, 0);
		sub_1412D66F0(
			&this->imageSpaceShaderParam,
			0,
			1.0 / (float)(int)BSGraphics::RenderTargetManager::singleton.renderTargetData[BSGraphics_RenderTarget_local_map_swap].width,
			1.0 / (float)(int)BSGraphics::RenderTargetManager::singleton.renderTargetData[BSGraphics_RenderTarget_local_map_swap].height,
			0,
			0);

		TESImageSpaceManager::sub_1412979E0(
			TESImageSpaceManager::pSingleton,
			0x62u,
			BSGraphics_RenderTarget_local_map_swap,
			BSGraphics_RenderTarget_local_map,
			&this->imageSpaceShaderParam);
		if ((_BYTE)v6) {
			v4->flags |= NiAVObject_Flag_Hidden;
		} else {
			v4->flags &= ~NiAVObject_Flag_Hidden;
		}
		shadowSceneNode->unk219 = unk219;
		byte_141E0E350 = v32;
		byte_1431D1D30 = v7;
		byte_141E0DC5C = v8;
		byte_141E0DC5D = v8;
		dword_1431D0D8C = 0;
		sub_1412CCF90(this->shaderAccumulator, 0);
	}
}