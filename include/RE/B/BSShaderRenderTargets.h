#pragma once

namespace RE
{
	struct RENDER_TARGETS
	{
		enum RENDER_TARGET
		{
			kNone = static_cast<std::underlying_type_t<RENDER_TARGET>>(-1),
			kFrameBuffer = 0,
			kMain,
			kMainCopy,
			kMainOnlyAlpha,
			kNormalTAAMaskSSRMask,
			kNormalTAAMaskSSRMaskSwap,
			kNormalTAAMaskSSRMaskDownsampled,
			kMotionVector,
			kWaterDisplacement,
			kWaterDisplacementSwap,
			kWaterReflections,
			kWaterFlow,
			kUnderwaterMask,
			kRefractionNormals,
			kMenuBG,
			kPlayerFacegenTint,
			kLocalMap,
			kLocalMapSwap,
			kShadowMask,
			kGetHitBuffer,
			kGetHitBlurSwap,
			kBlurFullBuffer,
			kHDRBlurSwap,
			kLDRBlurSwap,
			kHDRBloom,
			kLDRDownsample0,
			kHDRDownsample0,
			kHDRDownsample1,
			kHDRDownsample2,
			kHDRDownsample3,
			kHDRDownsample4,
			kHDRDownsample5,
			kHDRDownsample6,
			kHDRDownsample7,
			kHDRDownsample8,
			kHDRDownsample9,
			kHDRDownsample10,
			kHDRDownsample11,
			kHDRDownsample12,
			kHDRDownsample13,
			kLensFlareVIS,
			kImagespaceTempCopy,
			kImagespaceTempCopy2,
			kImagespaceVolumetricLighting,
			kImagespaceVolumetricLightingPrevoius,
			kImagespaceVolumetricLightingCopy,
			kSAO,
			kSAODownscaled,
			kSAOCameraZMIPLevel0ESRAM,
			kSAOCameraZ,
			kSAOCameraZMIPLevel0,
			kSAOCameraZMIPLevel1,
			kSAOCameraZMIPLevel2,
			kSAOCameraZMIPLevel3,
			kSAOCameraZMIPLevel4,
			kSAOCameraZMIPLevel5,
			kSAOCameraZMIPLevel6,
			kSAOCameraZMIPLevel7,
			kSAOCameraZMIPLevel8,
			kSAOCameraZMIPLevel9,
			kSAOCameraZMIPLevel10,
			kSAOCameraZMIPLevel11,
			kSAORawAO,
			kSAORawAODownscaled,
			kSAORawAOPrevious,
			kSAORawAOPreviousDownscaled,
			kSAOTempBlur,
			kSAOTempBlurDownscaled,
			kIndirect,
			kIndirectDownscaled,
			kRAWIndirect,
			kRAWIndirectDownscaled,
			kRAWIndirectPrevious,
			kRAWIndirectPREVIOUS_Downscaled,
			kRAWIndirectSwap,
			kSaveGameScreenshot,
			kScreenShot,
			kVolumetricLightingHalfRes,
			kVolumetricLightingBlurHalfRes,
			kVolumetricLightingQuarterRes,
			kVolumetricLightingBlurQuarterRes,
			kTemporalAAAccumulation1,
			kTemporalAAAccumulation2,
			kTemporalAAUIAccumulation1,
			kTemporalAAUIAccumulation2,
			kTemporalAAMask,
			kTemporalAAWater1,
			kTemporalAAWater2,
			kRAWWater,
			kWater1,
			kWater2,
			kIBLensFlaresLightsFilter,
			kIBLensFlaresDownsample4x4xPing,
			kIBLensFlaresDownsample4x4xPong,
			kIBLensFlaresDownsample16x4yPing,
			kIBLensFlaresDownsample16x4yPong,
			kIBLensFlaresDownsample16x4yBlur,
			kIBLensFlaresDownsample16x4yBlurSwap,
			kIBLensFlaresDownsample32x4yPing,
			kIBLensFlaresDownsample32x4yPong,
			kIBLensFlaresDownsample32x4yBlur,
			kIBLensFlaresDownsample32x4yBlurSwap,
			kIBLensFlaresDownsample16x16xPing,
			kIBLensFlaresDownsample16x16xPong,
			kIBLensFlaresDownsample16x16xSwap,
			kBookText0,
			kBookText1,
			kBookText2,
			kBookText3,
			kSSR,
			kSSRRaw,
			kSSRBlurRed0,
			kSnowSpecAlpha,
			kSnowSwap,

			kTotal,
			kFrameBufferTotal = kMain,
		};
	};
	using RENDER_TARGET = RENDER_TARGETS::RENDER_TARGET;

	struct RENDER_TARGETS_CUBEMAP
	{
		enum RENDER_TARGET_CUBEMAP
		{
			kNone = static_cast<std::underlying_type_t<RENDER_TARGET_CUBEMAP>>(-1),
			kReflections = 0,

			kTotal,
		};
	};
	using RENDER_TARGET_CUBEMAP = RENDER_TARGETS_CUBEMAP::RENDER_TARGET_CUBEMAP;

	struct RENDER_TARGETS_3D
	{
		enum RENDER_TARGET_3D
		{
			kNONE = static_cast<std::underlying_type_t<RENDER_TARGET_3D>>(-1),
			kVOLUMETRIC_LIGHTING_ACCUMULATION = 0,
			kVOLUMETRIC_LIGHTING_ACCUMULATION_COPY,
			kVOLUMETRIC_LIGHTING_NOISE,

			kTotal,
		};
	};
	using RENDER_TARGET_3D = RENDER_TARGETS_3D::RENDER_TARGET_3D;

	struct RENDER_TARGETS_DEPTHSTENCIL
	{
		enum RENDER_TARGET_DEPTHSTENCIL
		{
			kNONE = static_cast<std::underlying_type_t<RENDER_TARGET_DEPTHSTENCIL>>(-1),
			kMAIN = 0,
			kMAIN_COPY,
			kSHADOWMAPS_ESRAM,
			kVOLUMETRIC_LIGHTING_SHADOWMAPS_ESRAM,
			kSHADOWMAPS,
			kDECAL_OCCLUSION,
			kCUBEMAP_REFLECTIONS,
			kPOST_ZPREPASS_COPY,
			kPOST_WATER_COPY,
			kBOOK_TEXT,
			kPRECIPITATION_OCCLUSION_MAP,
			kFOCUS_NEO,

			kTotal,
		};
	};
	using RENDER_TARGET_DEPTHSTENCIL = RENDER_TARGETS_DEPTHSTENCIL::RENDER_TARGET_DEPTHSTENCIL;

	namespace BSGraphics
	{
		struct RenderTargetProperties
		{
			std::uint32_t width;		  // 00
			std::uint32_t height;		  // 04
			std::uint32_t format;		  // 08
			bool copyable;				  // 0C
			bool supportUnorderedAccess;  // 0D
			bool allowMipGeneration;	  // 0E
			bool unk0F;					  // 0F
			std::int32_t mipLevel;		  // 10
			std::uint32_t textureTarget;  // 14
			std::uint32_t unk18;		  // 18
		};
		static_assert(sizeof(RenderTargetProperties) == 0x1C);

		struct DepthStencilTargetProperties
		{
			std::uint32_t width;	  // 00
			std::uint32_t height;	  // 04
			std::uint32_t arraySize;  // 08
			bool unk0C;				  // 0C
			bool stencil;			  // 0D
			bool use16BitsDepth;	  // 0E
			std::uint8_t pad0F;		  // 0F
		};
		static_assert(sizeof(DepthStencilTargetProperties) == 0x10);

		struct CubeMapRenderTargetProperties
		{
			std::uint32_t width;   // 0
			std::uint32_t height;  // 4
			std::uint32_t format;  // 8
		};
		static_assert(sizeof(CubeMapRenderTargetProperties) == 0xC);
	}
}
