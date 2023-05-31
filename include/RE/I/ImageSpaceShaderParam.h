#pragma once

#include "RE/I/ImageSpaceEffectParam.h"
#include "RE/N/NiSmartPointer.h"
#include "RE/N/NiTArray.h"
#include "RE/N/NiTexture.h"

namespace RE
{
	class ImageSpaceShaderParam : public ImageSpaceEffectParam
	{
	public:
		inline static constexpr auto RTTI = RTTI_ImageSpaceShaderParam;

		~ImageSpaceShaderParam() override;	// 00

		 void sub_1412D66F0(std::uint32_t a_unk0, float a_invWidth, float a_invHeight, float a_unk3, float a_unk4)
		{
			using func_t = decltype(&ImageSpaceShaderParam::sub_1412D66F0);
			REL::Relocation<func_t> func{ RELOCATION_ID(100198, 0) };

			func(this, a_unk0, a_invWidth, a_invHeight, a_unk3, a_unk4);
		}

		// members
		std::uint64_t unk08;						 // 08
		std::uint64_t unk10;						 // 10
		std::uint64_t unk18;						 // 18
		std::uint64_t unk20;						 // 20
		std::uint64_t unk28;						 // 28
		void* unk30;								 // 30
		std::uint64_t unk38;						 // 38
		void* unk40;								 // 40
		std::uint64_t unk48;						 // 48
		NiTObjectArray<NiPointer<NiTexture>> unk50;	 // 50
		NiTPrimitiveArray<std::uint32_t> unk68;		 // 68 - NiTPrimitiveArray<enum BSGraphics::TextureFilterMode>
	};
	static_assert(sizeof(ImageSpaceShaderParam) == 0x80);
}
