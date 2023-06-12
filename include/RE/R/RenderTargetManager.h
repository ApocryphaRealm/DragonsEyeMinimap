#pragma once

#include "RE/B/BSShaderRenderTargets.h"
#include "RE/R/RenderTargetProperties.h"
#include "RE/R/RendererShadowState.h"

namespace RE
{
	namespace BSGraphics
	{
		class RenderTargetManager
		{
		public:
			static RenderTargetManager* GetSingleton()
			{
				REL::Relocation<RenderTargetManager*> instance{ RELOCATION_ID(524970, 411451) };
				return instance.get();
			}

			[[nodiscard]] int GetDepthStencil() const
			{
				using func_t = decltype(&RenderTargetManager::GetDepthStencil);
				REL::Relocation<func_t> func{ RELOCATION_ID(75650, 0) };

				return func(this);
			}

			void SetModeForRenderTarget(std::uint32_t a_renderTargetIndex, RENDER_TARGET a_renderTarget, SetRenderTargetMode a_renderTargetMode, bool a_unk3)
			{
				using func_t = decltype(&RenderTargetManager::SetModeForRenderTarget);
				REL::Relocation<func_t> func{ RELOCATION_ID(75646, 0) };

				func(this, a_renderTargetIndex, a_renderTarget, a_renderTargetMode, a_unk3);
			}

			void sub_140D74D10(std::int32_t a_depthStencil, std::int32_t a_depthStencilMode, std::int32_t a_depthStencilSlice, bool a_unk4)
			{
				using func_t = decltype(&RenderTargetManager::sub_140D74D10);
				REL::Relocation<func_t> func{ RELOCATION_ID(75647, 0) };

				func(this, a_depthStencil, a_depthStencilMode, a_depthStencilSlice, a_unk4);
			}

			// members
			RenderTargetProperties renderTargetData[RENDER_TARGET::kTOTAL];							   // 000
			DepthStencilTargetProperties depthStencilTargetData[RENDER_TARGETS_DEPTHSTENCIL::kTOTAL];  // C78
			CubeMapRenderTargetProperties cubeMapRenderTargetData[RENDER_TARGETS_CUBEMAP::kTOTAL];	   // D38
		};
		static_assert(sizeof(RenderTargetManager) == 0xD44);
	}
}
