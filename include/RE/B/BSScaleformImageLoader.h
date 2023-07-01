#pragma once

#include "RE/G/GFxImageLoader.h"
#include "RE/I/ImageData.h"

namespace RE
{
	class BSScaleformImageLoader : public GFxImageLoader
	{
	public:

		~BSScaleformImageLoader() override = default;  // 00

		// override (GFxImageLoader)
		GImageInfoBase* LoadImage(const char* a_imageUrl) override;	// 01

		bool AddVirtualImage(ImageData& a_image)
		{
			using func_t = decltype(&BSScaleformImageLoader::AddVirtualImage);
			REL::Relocation<func_t> func{ RELOCATION_ID(82382, 84469) };

			return func(this, a_image);
		}

		bool ReleaseVirtualImage(ImageData& a_image)
		{
			using func_t = decltype(&BSScaleformImageLoader::ReleaseVirtualImage);
			REL::Relocation<func_t> func{ RELOCATION_ID(82383, 84470) };

			return func(this, a_image);
		}

		// members
		std::uint64_t unk18;  // 18
		std::uint64_t unk20;  // 20
		std::uint64_t unk28;  // 28
		std::uint64_t unk30;  // 30
		std::uint64_t unk38;  // 38
		std::uint64_t unk40;  // 40
	};
	static_assert(sizeof(BSScaleformImageLoader) == 0x48);
}
