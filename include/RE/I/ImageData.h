#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/B/BSShaderRenderTargets.h"
#include "RE/N/NiSourceTexture.h"

namespace RE
{
	struct ImageData
	{
		bool BindToVirtualTarget(const BSFixedString& a_filePath, RENDER_TARGET a_renderTarget)
		{
			using func_t = decltype(&ImageData::BindToVirtualTarget);
			REL::Relocation<func_t> func{ RELOCATION_ID(80296, 82319) };

			return func(this, a_filePath, a_renderTarget);
		}

		// members
		NiPointer<NiSourceTexture> texture;			 // 00
		RENDER_TARGET			   renderTarget;	 // 08
		std::uint32_t			   pad0C;			 // 0C
		BSFixedString			   filePath;		 // 10
	};
	static_assert(sizeof(ImageData) == 0x18);
}
