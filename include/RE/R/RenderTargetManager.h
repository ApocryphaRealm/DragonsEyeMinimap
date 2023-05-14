#pragma once

#include "RE/B/BSShaderRenderTargets.h"

namespace RE
{
	namespace BSGraphics
	{
		class RenderTargetManager
		{
		public:
			static RenderTargetManager* GetSingleton()
			{
				REL::Relocation<RenderTargetManager*> instance{ RELOCATION_ID(524970, 0) };
				return instance.get();
			}

			[[nodiscard]] int GetDepthStencil() const
			{
				using func_t = decltype(&RenderTargetManager::GetDepthStencil);
				REL::Relocation<func_t> func{ RELOCATION_ID(75650, 0) };

				return func(this);
			}

			void sub_140D74CF0(std::int32_t a_unk0, RENDER_TARGET a_renderTarget, std::int32_t a_unk2, bool a_unk3)
			{
				using func_t = decltype(&RenderTargetManager::sub_140D74CF0);
				REL::Relocation<func_t> func{ RELOCATION_ID(75646, 0) };

				func(this, a_unk0, a_renderTarget, a_unk2, a_unk3);
			}

			void sub_140D74D10(std::int32_t a_depthStencil, std::int32_t a_depthStencilMode, std::int32_t a_depthStencilSlice)
			{
				using func_t = decltype(&RenderTargetManager::sub_140D74D10);
				REL::Relocation<func_t> func{ RELOCATION_ID(75647, 0) };

				func(this, a_depthStencil, a_depthStencilMode, a_depthStencilSlice);
			}

			// members
			RenderTargetProperties renderTargetData[RENDER_TARGET::kTotal];							  // 000
			DepthStencilTargetProperties depthStencilTargetData[RENDER_TARGET_DEPTHSTENCIL::kTotal];  // C78
			CubeMapRenderTargetProperties cubeMapRenderTargetData[RENDER_TARGET_CUBEMAP::kTotal];	  // D38
		};
		static_assert(sizeof(RenderTargetManager) == 0xD44);
	}
}
