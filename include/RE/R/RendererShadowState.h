#pragma once

#include "RE/D/DepthStencilDepthModes.h"
#include "RE/N/NiPoint2.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiRect.h"

namespace RE
{
	struct D3D11_VIEWPORT
	{
		float TopLeftX;
		float TopLeftY;
		float Width;
		float Height;
		float MinDepth;
		float MaxDepth;
	};

	struct ID3D11ShaderResourceView;
	struct ID3D11UnorderedAccessView;

	using D3D11_PRIMITIVE_TOPOLOGY = std::uint32_t;

	namespace DirectX
	{
		using XMVECTOR = __m128;

		struct XMMATRIX
		{
			XMVECTOR r[4];
		};
	}

	using NiRectF = NiRect<float>;

	namespace BSGraphics
	{
		class VertexShader;
		class PixelShader;

		enum SetRenderTargetMode : std::uint32_t
		{
			kClear = 0,
			lClearDepth = 1,
			kClearStencil = 2,
			kRestore = 3,
			kNoClear = 4,
			kForceCopyRestore = 5,
			kInit = 6,
		};

		class ViewData
		{
		public:

			// members
			DirectX::XMVECTOR viewUp;
			DirectX::XMVECTOR viewRight;
			DirectX::XMVECTOR viewDir;
			DirectX::XMMATRIX viewMat;
			DirectX::XMMATRIX projMat;
			DirectX::XMMATRIX viewProjMat;
			DirectX::XMMATRIX unknownMat1;
			DirectX::XMMATRIX viewProjMatrixUnjittered;
			DirectX::XMMATRIX previousViewProjMatrixUnjittered;
			DirectX::XMMATRIX projMatrixUnjittered;
			DirectX::XMMATRIX unknownMat2;
			NiRectF viewPort;
			NiPoint2 viewDepthRange;
		};

		class RendererShadowState
		{
		public:
			static RendererShadowState* GetSingleton()
			{
				REL::Relocation<RendererShadowState*> instance{ RELOCATION_ID(524773, 0) };
				return instance.get();
			}
			
			// members
			std::uint32_t stateUpdateFlags;
			std::uint32_t psResourceModifiedBits;
			std::uint32_t psSamplerModifiedBits;
			std::uint32_t csResourceModifiedBits;
			std::uint32_t csSamplerModifiedBits;
			std::uint32_t csUAVModifiedBits;
			std::uint32_t renderTargets[8];
			std::uint32_t depthStencil;
			std::uint32_t depthStencilSlice;
			std::uint32_t cubeMapRenderTarget;
			std::uint32_t cubeMapRenderTargetView;
			SetRenderTargetMode setRenderTargetMode[8];
			SetRenderTargetMode setDepthStencilMode;
			SetRenderTargetMode setCubeMapRenderTargetMode;
			D3D11_VIEWPORT viewPort;
			DepthStencilDepthMode depthStencilDepthMode;
			std::uint32_t depthStencilUnknown;
			std::uint32_t depthStencilStencilMode;
			std::uint32_t stencilRef;
			std::uint32_t rasterStateFillMode;
			std::uint32_t rasterStateCullMode;
			std::uint32_t rasterStateDepthBiasMode;
			std::uint32_t rasterStateScissorMode;
			std::uint32_t alphaBlendMode;
			std::uint32_t alphaBlendAlphaToCoverage;
			std::uint32_t alphaBlendWriteMode;
			bool alphaTestEnabled;
			float alphaTestRef;
			std::uint32_t psTextureAddressMode[16];
			std::uint32_t psTextureFilterMode[16];
			ID3D11ShaderResourceView* psTexture[16];
			std::uint32_t csTextureAddressMode[16];
			std::uint32_t csTextureFilterMode[16];
			ID3D11ShaderResourceView* csTexture[16];
			std::uint32_t csTextureMinLodMode[16];
			ID3D11UnorderedAccessView* csUAV[8];
			uint64_t vertexDesc;
			VertexShader* currentVertexShader;
			PixelShader* currentPixelShader;
			D3D11_PRIMITIVE_TOPOLOGY topology;
			NiPoint3 posAdjust;
			NiPoint3 previousPosAdjust;
			ViewData cameraData;
			std::uint32_t alphaBlendModeExtra;
		};
		static_assert(sizeof(RendererShadowState) == 0x5E0);

	}
}