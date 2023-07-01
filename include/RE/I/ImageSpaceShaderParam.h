#pragma once

#include "RE/I/ImageSpaceEffectParam.h"
#include "RE/N/NiSmartPointer.h"
#include "RE/N/NiTArray.h"
#include "RE/N/NiTexture.h"
#include "RE/T/TextureFilterModes.h"

namespace RE
{
	class ImageSpaceShaderParam : public ImageSpaceEffectParam
	{
	public:
		inline static constexpr auto RTTI = RTTI_ImageSpaceShaderParam;

		~ImageSpaceShaderParam() override;	// 00

		 void SetupPixelConstantGroup(std::uint32_t a_pixelCGIndex, float a_invWidth,
									  float a_invHeight, float a_unk3, float a_unk4)
		{
			using func_t = decltype(&ImageSpaceShaderParam::SetupPixelConstantGroup);
			REL::Relocation<func_t> func{ RELOCATION_ID(100198, 106905) };

			func(this, a_pixelCGIndex, a_invWidth, a_invHeight, a_unk3, a_unk4);
		}

		// members
		std::uint32_t                                    unk08;                    // 08
		bool                                             SLIMode;                  // 0C
		std::uint8_t                                     pad0D;                    // 0D
		std::uint16_t                                    pad0E;                    // 0E
		std::uint64_t                                    unk10;                    // 10
		std::uint64_t                                    unk18;                    // 18
		std::uint8_t                                     unk20;                    // 20
		std::uint8_t                                     pad21;                    // 21
		std::uint16_t                                    pad22;                    // 22
		std::uint32_t                                    unk24;                    // 24
		float                                            unk28;                    // 28
		std::uint32_t                                    unk2C;                    // 2C
		float*                                           vertexConstantGroup;      // 30
		std::uint32_t                                    vertexConstantGroupSize;  // 38
		std::uint32_t                                    unk3C;                    // 3C
		float*                                           pixelConstantGroup;       // 40
		std::uint32_t                                    pixelConstantGroupSize;   // 48
		std::uint32_t                                    unk4C;                    // 4C
		NiTObjectArray<NiPointer<NiTexture>>             textures;                 // 50
		NiTPrimitiveArray<BSGraphics::TextureFilterMode> textureFilterModes;       // 68
	};
	static_assert(sizeof(ImageSpaceShaderParam) == 0x80);
}
