#include "Minimap.h"

#include "RE/R/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <d3d11.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

// INTERIOR CACHE (1.8.0). An interior's map does not change with the player, so its geometry is drawn once and kept.
//
// What the game's pass is made of decides what can be kept. Reading it (WorldRendering.cpp and Local Map Upgrade's
// ISLocalMap.hlsl): the geometry goes into kLOCAL_MAP_SWAP; a fog pass writes that target's alpha; ISLocalMap then turns
// it into kLOCAL_MAP, adding the edge lines, the colour or black-and-white style, the player's minimum opacity, the fog,
// and the round or square fade, all in the coordinates of the picture it is given. So kLOCAL_MAP cannot be the thing that
// is stored: it has the fade and the fog baked in. What is stored is the raw geometry, kLOCAL_MAP_SWAP before any of that.
//
// Capture: the camera is framed on one tile at a time, north up, through the engine's own camera update; RenderOffScreen
// draws the geometry and stops; the result is copied into this texture. One tile a frame, so there is no hitch. Tiles
// sit edge to edge because nothing that looks at neighbouring pixels has run yet.
//
// Compose: the part of the texture the live camera sees is drawn into kLOCAL_MAP_SWAP with one textured triangle, turned
// and scaled by what the two cameras themselves report. The fog pass and ISLocalMap then run as they always did. No cull
// of the cell, no geometry draw.
//
// Height: the engine's camera sits at the top of the area bounds whatever the player's height (LocalMapCamera::
// DefaultState::Update, 1.5.97 rva 0x1F6300), so the live map has never had floors; a single picture is what it draws.
namespace DEM
{
	namespace
	{
		constexpr std::uint64_t kMaxBytes = 128ull * 1024 * 1024;   // mips included
		constexpr std::uint32_t kMaxDimension = 8192;
		constexpr float kMaxSoften = 2.0F;                          // the most a big cell's tiles may grow before it stays live
		constexpr float kMaxMagnification = 1.25F;                  // stored pixels stretched more than this: draw live instead
		constexpr auto kSettle = std::chrono::milliseconds(1200);   // after the last door, cell or settings event
		constexpr auto kMaxAge = std::chrono::seconds(30);          // the safety net for what no event announces

		ID3D11Texture2D* g_texture = nullptr;
		ID3D11ShaderResourceView* g_srv = nullptr;
		ID3D11VertexShader* g_vs = nullptr;
		ID3D11PixelShader* g_ps = nullptr;
		ID3D11Buffer* g_constants = nullptr;
		ID3D11SamplerState* g_sampler = nullptr;
		ID3D11RasterizerState* g_raster = nullptr;
		ID3D11DepthStencilState* g_depth = nullptr;
		ID3D11BlendState* g_blend = nullptr;
		bool g_drawObjectsFailed = false;

		constexpr const char* kShader = R"(
cbuffer Map : register(b0) { float4 RowU; float4 RowV; };
Texture2D<float4> Picture : register(t0);
SamplerState PictureSampler : register(s0);
struct V { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
V VS(uint id : SV_VertexID)
{
	V o;
	float2 t = float2((id << 1) & 2, id & 2);
	o.pos = float4(t.x * 2.0 - 1.0, 1.0 - t.y * 2.0, 0.0, 1.0);
	o.uv = float2(dot(RowU.xyz, float3(t, 1.0)), dot(RowV.xyz, float3(t, 1.0)));
	return o;
}
float4 PS(V i) : SV_Target0 { return Picture.Sample(PictureSampler, i.uv); }
)";

		struct RenderTargetViews
		{
			ID3D11Device* device = nullptr;
			ID3D11DeviceContext* context = nullptr;
			ID3D11Texture2D* swapTexture = nullptr;
			ID3D11RenderTargetView* swapRTV = nullptr;
			ID3D11ShaderResourceView* swapSRV = nullptr;
		};

		RenderTargetViews Views()
		{
			RenderTargetViews v;
#if RUNTIME_LINE == 17
			auto* data = RE::BSGraphics::Renderer::GetRendererDataSingleton();
			if (!data) { return v; }
#else
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!renderer) { return v; }
			auto* data = &renderer->GetRuntimeData();
#endif
			v.device = reinterpret_cast<ID3D11Device*>(data->forwarder);
			v.context = reinterpret_cast<ID3D11DeviceContext*>(data->context);
			const auto& swap = data->renderTargets[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];
			v.swapTexture = reinterpret_cast<ID3D11Texture2D*>(swap.texture);
			v.swapRTV = reinterpret_cast<ID3D11RenderTargetView*>(swap.RTV);
			v.swapSRV = reinterpret_cast<ID3D11ShaderResourceView*>(swap.SRV);
			return v;
		}

		template <class T>
		void Release(T*& a_p)
		{
			if (a_p)
			{
				a_p->Release();
				a_p = nullptr;
			}
		}

		ID3DBlob* Compile(const char* a_entry, const char* a_target)
		{
			ID3DBlob* code = nullptr;
			ID3DBlob* errors = nullptr;
			if (FAILED(D3DCompile(kShader, std::strlen(kShader), "DEMInteriorCompose", nullptr, nullptr, a_entry, a_target,
					D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors)))
			{
				logger::warn("Interior cache: the compose shader {} did not compile: {}", a_entry,
					errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
				Release(code);
			}
			Release(errors);
			return code;
		}

		bool EnsureDrawObjects(ID3D11Device* a_device)
		{
			if (g_ps) { return true; }
			if (g_drawObjectsFailed) { return false; }

			ID3DBlob* vs = Compile("VS", "vs_5_0");
			ID3DBlob* ps = Compile("PS", "ps_5_0");
			bool ok = vs && ps;
			ok = ok && SUCCEEDED(a_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs));
			ID3D11PixelShader* pixelShader = nullptr;
			ok = ok && SUCCEEDED(a_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixelShader));
			Release(vs);
			Release(ps);

			D3D11_BUFFER_DESC cb{};
			cb.ByteWidth = 32;
			cb.Usage = D3D11_USAGE_DYNAMIC;
			cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			ok = ok && SUCCEEDED(a_device->CreateBuffer(&cb, nullptr, &g_constants));

			// Outside the picture: what the geometry pass clears to, so ISLocalMap treats it as it treats empty space.
			D3D11_SAMPLER_DESC sd{};
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
			sd.BorderColor[3] = 1.0F;
			sd.MaxLOD = D3D11_FLOAT32_MAX;
			ok = ok && SUCCEEDED(a_device->CreateSamplerState(&sd, &g_sampler));

			D3D11_RASTERIZER_DESC rd{};
			rd.FillMode = D3D11_FILL_SOLID;
			rd.CullMode = D3D11_CULL_NONE;
			ok = ok && SUCCEEDED(a_device->CreateRasterizerState(&rd, &g_raster));

			D3D11_DEPTH_STENCIL_DESC dd{};
			ok = ok && SUCCEEDED(a_device->CreateDepthStencilState(&dd, &g_depth));

			D3D11_BLEND_DESC bd{};
			bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			ok = ok && SUCCEEDED(a_device->CreateBlendState(&bd, &g_blend));

			if (!ok)
			{
				logger::warn("Interior cache: the compose objects could not be created; interiors stay live this session");
				Release(pixelShader);
				g_drawObjectsFailed = true;
				return false;
			}
			g_ps = pixelShader;   // last, so g_ps alone says "everything is there"
			return true;
		}

		bool EnsureTexture(const RenderTargetViews& a_views, std::uint32_t a_width, std::uint32_t a_height)
		{
			if (g_texture)
			{
				D3D11_TEXTURE2D_DESC have{};
				g_texture->GetDesc(&have);
				if (have.Width == a_width && have.Height == a_height) { return true; }
				Release(g_srv);
				Release(g_texture);
			}

			D3D11_TEXTURE2D_DESC swapDesc{};
			a_views.swapTexture->GetDesc(&swapDesc);
			D3D11_SHADER_RESOURCE_VIEW_DESC swapView{};
			a_views.swapSRV->GetDesc(&swapView);

			D3D11_TEXTURE2D_DESC td{};
			td.Width = a_width;
			td.Height = a_height;
			td.MipLevels = 0;
			td.ArraySize = 1;
			td.Format = swapDesc.Format;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;   // GenerateMips asks for both
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			if (swapDesc.SampleDesc.Count != 1 || FAILED(a_views.device->CreateTexture2D(&td, nullptr, &g_texture)))
			{
				logger::warn("Interior cache: no {}x{} texture (format {}, samples {}); this interior stays live", a_width, a_height,
					static_cast<int>(swapDesc.Format), swapDesc.SampleDesc.Count);
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
			vd.Format = swapView.Format;
			vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			vd.Texture2D.MipLevels = static_cast<UINT>(-1);
			if (FAILED(a_views.device->CreateShaderResourceView(g_texture, &vd, &g_srv)))
			{
				logger::warn("Interior cache: no view of the {}x{} texture; this interior stays live", a_width, a_height);
				Release(g_texture);
				return false;
			}
			return true;
		}

		// Every piece of device state the draw touches goes back exactly as it was, including the shader resources: binding
		// kLOCAL_MAP_SWAP as a target makes D3D drop it from any resource slot behind the engine's back, and the engine's
		// state tracking would then skip re-binding it for ISLocalMap.
		bool DrawPicture(const RenderTargetViews& a_views, const float a_rows[8])
		{
			ID3D11DeviceContext* c = a_views.context;

			D3D11_TEXTURE2D_DESC desc{};
			a_views.swapTexture->GetDesc(&desc);

			ID3D11RenderTargetView* savedRTV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
			ID3D11DepthStencilView* savedDSV = nullptr;
			ID3D11BlendState* savedBlend = nullptr;
			FLOAT savedBlendFactor[4]{};
			UINT savedSampleMask = 0;
			ID3D11DepthStencilState* savedDepth = nullptr;
			UINT savedStencilRef = 0;
			ID3D11RasterizerState* savedRaster = nullptr;
			UINT savedViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			D3D11_VIEWPORT savedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
			D3D11_PRIMITIVE_TOPOLOGY savedTopology{};
			ID3D11InputLayout* savedLayout = nullptr;
			ID3D11VertexShader* savedVS = nullptr;
			ID3D11GeometryShader* savedGS = nullptr;
			ID3D11HullShader* savedHS = nullptr;
			ID3D11DomainShader* savedDomain = nullptr;
			ID3D11PixelShader* savedPS = nullptr;
			ID3D11Buffer* savedVSConstants = nullptr;
			ID3D11Buffer* savedPSConstants = nullptr;
			ID3D11SamplerState* savedSampler = nullptr;
			ID3D11ShaderResourceView* savedPSResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
			ID3D11ShaderResourceView* savedVSResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};

			c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTV, &savedDSV);
			c->OMGetBlendState(&savedBlend, savedBlendFactor, &savedSampleMask);
			c->OMGetDepthStencilState(&savedDepth, &savedStencilRef);
			c->RSGetState(&savedRaster);
			c->RSGetViewports(&savedViewportCount, savedViewports);
			c->IAGetPrimitiveTopology(&savedTopology);
			c->IAGetInputLayout(&savedLayout);
			c->VSGetShader(&savedVS, nullptr, nullptr);
			c->GSGetShader(&savedGS, nullptr, nullptr);
			c->HSGetShader(&savedHS, nullptr, nullptr);
			c->DSGetShader(&savedDomain, nullptr, nullptr);
			c->PSGetShader(&savedPS, nullptr, nullptr);
			c->VSGetConstantBuffers(0, 1, &savedVSConstants);
			c->PSGetConstantBuffers(0, 1, &savedPSConstants);
			c->PSGetSamplers(0, 1, &savedSampler);
			c->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, savedPSResources);
			c->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, savedVSResources);

			D3D11_MAPPED_SUBRESOURCE mapped{};
			const bool mappedOk = SUCCEEDED(c->Map(g_constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			if (mappedOk)
			{
				std::memcpy(mapped.pData, a_rows, sizeof(float) * 8);
				c->Unmap(g_constants, 0);

				const D3D11_VIEWPORT viewport{ 0.0F, 0.0F, static_cast<FLOAT>(desc.Width), static_cast<FLOAT>(desc.Height), 0.0F, 1.0F };
				ID3D11ShaderResourceView* nothing = nullptr;
				for (UINT slot = 0; slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++slot)
				{
					if (savedPSResources[slot]) { c->PSSetShaderResources(slot, 1, &nothing); }
				}

				c->OMSetRenderTargets(1, &a_views.swapRTV, nullptr);
				c->OMSetBlendState(g_blend, nullptr, 0xFFFFFFFF);
				c->OMSetDepthStencilState(g_depth, 0);
				c->RSSetState(g_raster);
				c->RSSetViewports(1, &viewport);
				c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				c->IASetInputLayout(nullptr);
				c->VSSetShader(g_vs, nullptr, 0);
				c->GSSetShader(nullptr, nullptr, 0);
				c->HSSetShader(nullptr, nullptr, 0);
				c->DSSetShader(nullptr, nullptr, 0);
				c->PSSetShader(g_ps, nullptr, 0);
				c->VSSetConstantBuffers(0, 1, &g_constants);
				c->PSSetSamplers(0, 1, &g_sampler);
				c->PSSetShaderResources(0, 1, &g_srv);

				c->Draw(3, 0);
			}

			c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTV, savedDSV);
			c->OMSetBlendState(savedBlend, savedBlendFactor, savedSampleMask);
			c->OMSetDepthStencilState(savedDepth, savedStencilRef);
			c->RSSetState(savedRaster);
			c->RSSetViewports(savedViewportCount, savedViewports);
			c->IASetPrimitiveTopology(savedTopology);
			c->IASetInputLayout(savedLayout);
			c->VSSetShader(savedVS, nullptr, 0);
			c->GSSetShader(savedGS, nullptr, 0);
			c->HSSetShader(savedHS, nullptr, 0);
			c->DSSetShader(savedDomain, nullptr, 0);
			c->PSSetShader(savedPS, nullptr, 0);
			c->VSSetConstantBuffers(0, 1, &savedVSConstants);
			c->PSSetConstantBuffers(0, 1, &savedPSConstants);
			c->PSSetSamplers(0, 1, &savedSampler);
			c->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, savedPSResources);
			c->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, savedVSResources);

			for (auto* p : savedRTV) { if (p) { p->Release(); } }
			if (savedDSV) { savedDSV->Release(); }
			if (savedBlend) { savedBlend->Release(); }
			if (savedDepth) { savedDepth->Release(); }
			if (savedRaster) { savedRaster->Release(); }
			if (savedLayout) { savedLayout->Release(); }
			if (savedVS) { savedVS->Release(); }
			if (savedGS) { savedGS->Release(); }
			if (savedHS) { savedHS->Release(); }
			if (savedDomain) { savedDomain->Release(); }
			if (savedPS) { savedPS->Release(); }
			if (savedVSConstants) { savedVSConstants->Release(); }
			if (savedPSConstants) { savedPSConstants->Release(); }
			if (savedSampler) { savedSampler->Release(); }
			for (auto* p : savedPSResources) { if (p) { p->Release(); } }
			for (auto* p : savedVSResources) { if (p) { p->Release(); } }

			return mappedOk;
		}

		// Where a world point lands in the camera's picture, as the engine itself works it out for the markers.
		struct PlaneMap
		{
			float xX = 0, xY = 0, x0 = 0;   // picture x (0-1) from world X and Y
			float yX = 0, yY = 0, y0 = 0;   // picture y (0-1, up) from world X and Y
		};

		PlaneMap MeasureCamera(RE::NiCamera* a_camera, float a_x, float a_y, float a_z)
		{
			constexpr float kStep = 100.0F;
			const auto project = [&](float x, float y, float& ox, float& oy) {
				float oz = 0.0F;
				a_camera->WorldPtToScreenPt3(a_camera->GetRuntimeData().worldToCam, a_camera->GetRuntimeData2().port,
					RE::NiPoint3{ x, y, a_z }, ox, oy, oz, 0.00001F);
			};

			float cx = 0, cy = 0, ax = 0, ay = 0, bx = 0, by = 0;
			project(a_x, a_y, cx, cy);
			project(a_x + kStep, a_y, ax, ay);
			project(a_x, a_y + kStep, bx, by);

			PlaneMap m;
			m.xX = (ax - cx) / kStep;
			m.yX = (ay - cy) / kStep;
			m.xY = (bx - cx) / kStep;
			m.yY = (by - cy) / kStep;
			m.x0 = cx - m.xX * a_x - m.xY * a_y;
			m.y0 = cy - m.yX * a_x - m.yY * a_y;
			return m;
		}
	}

	void Minimap::ResetInteriorCache(bool a_releaseTexture)
	{
		interiorCache = InteriorCacheState{};
		if (a_releaseTexture)
		{
			Release(g_srv);
			Release(g_texture);
		}
		perf::SetCacheInfo(0, 0, 0, "none");
	}

	void Minimap::ServiceInteriorCache(const PictureKey& a_key, std::chrono::steady_clock::time_point a_now)
	{
		InteriorCacheState& c = interiorCache;

		if (!settings::performance::interiorCache || a_key.interiorCell == 0)
		{
			if (c.cell != 0 || g_texture)
			{
				logger::debug("Interior cache: released ({})", a_key.interiorCell ? "switched off" : "left the interior");
				ResetInteriorCache(true);
			}
			return;
		}

		const float minHalfW = cameraContext->defaultState->minFrustumHalfWidth;
		const float minHalfH = cameraContext->defaultState->minFrustumHalfHeight;

		const auto sameBounds = [&](const RE::NiPoint3& p, const RE::NiPoint3& q) {
			return std::abs(p.x - q.x) < 1.0F && std::abs(p.y - q.y) < 1.0F && std::abs(p.z - q.z) < 1.0F;
		};

		// A new cell, a new resolution, or a camera frame of another shape (the display scale changes it): start over.
		if (c.cell != a_key.interiorCell || c.targetWidth != a_key.targetWidth || c.targetHeight != a_key.targetHeight ||
			!sameBounds(c.boundsA, a_key.boundsA) || !sameBounds(c.boundsB, a_key.boundsB) ||
			std::abs(c.minHalfW - minHalfW) > 0.5F || std::abs(c.minHalfH - minHalfH) > 0.5F)
		{
			ResetInteriorCache(false);
			c.cell = a_key.interiorCell;
			c.targetWidth = a_key.targetWidth;
			c.targetHeight = a_key.targetHeight;
			c.boundsA = a_key.boundsA;
			c.boundsB = a_key.boundsB;
			c.minHalfW = minHalfW;
			c.minHalfH = minHalfH;
			c.seenEpoch = cacheEpoch.load(std::memory_order_relaxed);
			c.dirty = true;
			c.changedAt = a_now;

			const float minX = std::min(a_key.boundsA.x, a_key.boundsB.x);
			const float maxX = std::max(a_key.boundsA.x, a_key.boundsB.x);
			const float minY = std::min(a_key.boundsA.y, a_key.boundsB.y);
			const float maxY = std::max(a_key.boundsA.y, a_key.boundsB.y);

			if (maxX - minX < 1.0F || maxY - minY < 1.0F || minHalfW < 1.0F || minHalfH < 1.0F || c.targetWidth == 0 || c.targetHeight == 0)
			{
				c.state = "notReady";   // the bound is not built yet: the comparison above starts over when it is
				return;
			}

			// A tile is what the camera frames at its closest zoom, so the stored picture is as sharp there as the live
			// one. A cell too big for the cap gets bigger tiles, up to kMaxSoften; beyond that it stays live.
			float soften = 1.0F;
			for (;;)
			{
				c.tileWorldW = 2.0F * minHalfW * soften;
				c.tileWorldH = 2.0F * minHalfH * soften;
				c.tilesX = std::max(1u, static_cast<std::uint32_t>(std::ceil((maxX - minX) / c.tileWorldW)));
				c.tilesY = std::max(1u, static_cast<std::uint32_t>(std::ceil((maxY - minY) / c.tileWorldH)));

				const std::uint64_t w = std::uint64_t{ c.tilesX } * c.targetWidth;
				const std::uint64_t h = std::uint64_t{ c.tilesY } * c.targetHeight;
				c.bytes = w * h * 4 * 4 / 3;   // RGBA8, a third more for the mips
				if (c.bytes <= kMaxBytes && w <= kMaxDimension && h <= kMaxDimension) { break; }

				soften *= 1.25F;
				if (soften > kMaxSoften)
				{
					c.state = "tooLarge";
					logger::info("Interior cache: cell {:X} spans {:.0f} x {:.0f} units, more than the {} MB cap holds at a usable sharpness; it stays live",
						c.cell, maxX - minX, maxY - minY, kMaxBytes / (1024 * 1024));
					perf::SetCacheInfo(0, 0, 0, c.state);
					return;
				}
			}

			c.originX = (minX + maxX) * 0.5F - c.tilesX * c.tileWorldW * 0.5F;
			c.originY = (minY + maxY) * 0.5F - c.tilesY * c.tileWorldH * 0.5F;
			c.planned = true;
			c.state = "waiting";

			logger::debug("Interior cache: cell {:X} spans {:.0f} x {:.0f} units; {} x {} tiles of {:.0f} x {:.0f} units (softened {:.2f}), {:.1f} MB",
				c.cell, maxX - minX, maxY - minY, c.tilesX, c.tilesY, c.tileWorldW, c.tileWorldH, soften, static_cast<double>(c.bytes) / (1024.0 * 1024.0));
		}

		if (!c.planned || c.failed)
		{
			return;
		}

		const std::uint32_t epoch = cacheEpoch.load(std::memory_order_relaxed);
		if (epoch != c.seenEpoch)
		{
			c.seenEpoch = epoch;
			c.dirty = true;
			c.changedAt = a_now;
		}
		if (c.valid && !c.dirty && !c.capturing && a_now - c.completedAt >= kMaxAge)
		{
			c.dirty = true;
			c.changedAt = a_now - kSettle;
		}

		if (!c.capturing && c.dirty && a_now - c.changedAt >= kSettle)
		{
			c.dirty = false;
			c.capturing = true;
			c.nextTile = 0;
			c.state = c.valid ? "refreshing" : "capturing";
		}

		if (c.capturing && !inputControlledMode)
		{
			CaptureInteriorTile(a_key);
		}

		perf::SetCacheInfo(c.bytes, c.tilesX, c.tilesY, c.state);
	}

	void Minimap::CaptureInteriorTile(const PictureKey& a_key)
	{
		InteriorCacheState& c = interiorCache;

		const RenderTargetViews views = Views();
		if (!views.device || !views.context || !views.swapTexture || !views.swapSRV || !views.swapRTV ||
			!EnsureDrawObjects(views.device) || !EnsureTexture(views, c.tilesX * c.targetWidth, c.tilesY * c.targetHeight))
		{
			c.failed = true;
			c.capturing = false;
			c.valid = false;
			c.state = "failed";
			return;
		}

		const std::uint32_t i = c.nextTile % c.tilesX;
		const std::uint32_t j = c.nextTile / c.tilesX;

		auto& state = *cameraContext->defaultState;
		const RE::NiPoint3 savedMax = cameraContext->maxExtent;
		const RE::NiPoint3 savedMin = cameraContext->minExtent;
		const RE::NiPoint3 savedInitial = state.initialPosition;
		const RE::NiPoint3 savedTranslation = state.translation;
		const float savedZoom = state.zoom;
		const float savedZoomInput = cameraContext->zoomInput;
		const float savedMinHalfW = state.minFrustumHalfWidth;
		const float savedMinHalfH = state.minFrustumHalfHeight;

		const float centreX = c.originX + (static_cast<float>(i) + 0.5F) * c.tileWorldW;
		const float centreY = c.originY + (static_cast<float>(j) + 0.5F) * c.tileWorldH;
		const float zHigh = std::max(savedMax.z, savedMin.z);
		const float zLow = std::min(savedMax.z, savedMin.z);

		// Fully zoomed out on bounds that are exactly this tile: the engine's own update then frames exactly this tile.
		cameraContext->maxExtent = RE::NiPoint3{ centreX + c.tileWorldW * 0.5F, centreY + c.tileWorldH * 0.5F, zHigh };
		cameraContext->minExtent = RE::NiPoint3{ centreX - c.tileWorldW * 0.5F, centreY - c.tileWorldH * 0.5F, zLow };
		state.initialPosition.x = centreX;
		state.initialPosition.y = centreY;
		state.translation = RE::NiPoint3::Zero();
		state.zoom = 0.0F;
		cameraContext->zoomInput = 0.0F;
		state.minFrustumHalfWidth = c.tileWorldW * 0.5F;
		state.minFrustumHalfHeight = c.tileWorldH * 0.5F;
		cameraContext->SetNorthRotation(0.0F);
		cameraContext->zRotation = 0.0F;
		cameraContext->Update();

		// Ask the camera what it actually frames rather than assuming: an axis-aligned tile centred on the picture, at the
		// expected scale. Anything else and this interior simply stays live.
		RE::NiCamera* camera = cameraContext->camera.get();
		const PlaneMap map = MeasureCamera(camera, centreX, centreY, (zHigh + zLow) * 0.5F);
		const float expectX = 1.0F / c.tileWorldW;
		const float expectY = 1.0F / c.tileWorldH;
		const float pictureX = map.xX * centreX + map.xY * centreY + map.x0;
		const float pictureY = map.yX * centreX + map.yY * centreY + map.y0;
		const bool framed = std::abs(std::abs(map.xX) - expectX) < expectX * 0.05F && std::abs(std::abs(map.yY) - expectY) < expectY * 0.05F &&
		                    std::abs(map.xY) < expectX * 0.05F && std::abs(map.yX) < expectY * 0.05F &&
		                    std::abs(pictureX - 0.5F) < 0.02F && std::abs(pictureY - 0.5F) < 0.02F;

		bool drew = false;
		if (framed)
		{
			// World X and Y run with or against the picture depending on the camera's handedness; the tile goes where its
			// own orientation puts it. Rows count downward, picture y upward.
			const std::uint32_t column = map.xX > 0.0F ? i : c.tilesX - 1 - i;
			const std::uint32_t row = map.yY > 0.0F ? c.tilesY - 1 - j : j;

			if (c.nextTile == 0)
			{
				// One mapping for the whole texture, from the first tile: texture u and v from world X and Y.
				c.uX = map.xX / static_cast<float>(c.tilesX);
				c.u0 = (map.x0 + static_cast<float>(column)) / static_cast<float>(c.tilesX);
				c.vY = -map.yY / static_cast<float>(c.tilesY);
				c.v0 = (1.0F - map.y0 + static_cast<float>(row)) / static_cast<float>(c.tilesY);
			}

			{
				const perf::ScopedTimer timer{ perf::Section::kCacheRender };
				drew = RenderOffScreen(PassMode::kCapture);
			}
			if (drew)
			{
				views.context->CopySubresourceRegion(g_texture, 0, column * c.targetWidth, row * c.targetHeight, 0, views.swapTexture, 0, nullptr);
			}
		}
		else
		{
			logger::warn("Interior cache: the camera did not frame the tile as asked (scale {:.6f}/{:.6f} of {:.6f}/{:.6f}, cross {:.6f}/{:.6f}, centre {:.3f},{:.3f}); this interior stays live",
				map.xX, map.yY, expectX, expectY, map.xY, map.yX, pictureX, pictureY);
		}

		// The camera goes back exactly as the frame's markers saw it.
		cameraContext->maxExtent = savedMax;
		cameraContext->minExtent = savedMin;
		state.initialPosition = savedInitial;
		state.translation = savedTranslation;
		state.zoom = savedZoom;
		state.minFrustumHalfWidth = savedMinHalfW;
		state.minFrustumHalfHeight = savedMinHalfH;
		cameraContext->zoomInput = 0.0F;
		cameraContext->SetNorthRotation(liveNorthRotation);
		cameraContext->zRotation = 0.0F;
		cameraContext->Update();
		cameraContext->zoomInput = savedZoomInput;

		if (!framed || !drew)
		{
			c.failed = true;
			c.capturing = false;
			c.valid = false;
			c.state = "failed";
			return;
		}

		if (++c.nextTile >= c.tilesX * c.tilesY)
		{
			views.context->GenerateMips(g_srv);
			c.capturing = false;
			c.valid = true;
			c.completedAt = std::chrono::steady_clock::now();
			c.state = "ready";
			BumpContentEpoch(perf::EpochCause::kOther);   // show it, and on a refresh replace what was there
			logger::debug("Interior cache: cell {:X} captured, {} tile(s)", c.cell, c.tilesX * c.tilesY);
		}
	}

	bool Minimap::InteriorCacheUsable(const PictureKey& a_key) const
	{
		const InteriorCacheState& c = interiorCache;
		return settings::performance::interiorCache && c.valid && !c.failed && g_srv && c.cell == a_key.interiorCell && a_key.frustumWidth > 0.0F &&
		       c.tileWorldW / a_key.frustumWidth <= kMaxMagnification;
	}

	bool Minimap::ComposeInteriorPicture(const PictureKey& a_key)
	{
		const InteriorCacheState& c = interiorCache;
		const RenderTargetViews views = Views();
		if (!views.context || !views.swapRTV || !views.swapTexture || !g_srv)
		{
			return false;
		}

		const PlaneMap live = MeasureCamera(cameraContext->camera.get(), a_key.player.x, a_key.player.y,
			(a_key.boundsA.z + a_key.boundsB.z) * 0.5F);
		const float det = live.xX * live.yY - live.xY * live.yX;
		if (std::abs(det) < 1e-12F)
		{
			return false;
		}

		// A position in the target (u right, v down) -> the live picture (y up) -> the world -> the stored texture.
		const auto toStored = [&](float u, float v, float& su, float& sv) {
			const float px = u - live.x0;
			const float py = (1.0F - v) - live.y0;
			const float worldX = (px * live.yY - py * live.xY) / det;
			const float worldY = (py * live.xX - px * live.yX) / det;
			su = c.uX * worldX + c.u0;
			sv = c.vY * worldY + c.v0;
		};

		float u00, v00, u10, v10, u01, v01;
		toStored(0.0F, 0.0F, u00, v00);
		toStored(1.0F, 0.0F, u10, v10);
		toStored(0.0F, 1.0F, u01, v01);

		const float rows[8] = { u10 - u00, u01 - u00, u00, 0.0F, v10 - v00, v01 - v00, v00, 0.0F };
		return DrawPicture(views, rows);
	}
}
