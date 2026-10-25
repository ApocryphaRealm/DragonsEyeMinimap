#include "RenderDiagnostics.h"

#include "utils/Logger.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <vector>

#if RUNTIME_LINE != 17
#	include <d3d11.h>
#	include "RE/R/Renderer.h"
#	include "RE/R/RenderTargetData.h"
#	include "RE/R/RenderTargetManager.h"
#	include "RE/S/State.h"
#endif

namespace rdiag
{
	namespace
	{
		std::atomic<int> g_dumpStage{ 0 };   // 0 idle, 1 = dump SWAP on the next geometry pass, 2 = then LOCAL_MAP
		std::mutex g_lock;
		std::string g_last = "{}";
		std::string g_stamp;
		std::string g_parts;

#if RUNTIME_LINE != 17
		float HalfToFloat(std::uint16_t h)
		{
			const std::uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1F, m = h & 0x3FF;
			if (e == 0) { return (s ? -1.0F : 1.0F) * std::ldexp(static_cast<float>(m), -24); }
			if (e == 31) { return m ? 0.0F : (s ? -65504.0F : 65504.0F); }
			return (s ? -1.0F : 1.0F) * std::ldexp(static_cast<float>(m | 0x400), static_cast<int>(e) - 25);
		}

		std::uint8_t To8(float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); }

		// one pixel of the formats the local-map targets can have, as 8-bit RGBA; false for a format not handled
		bool Pixel(DXGI_FORMAT a_fmt, const std::uint8_t* a_p, std::uint8_t a_out[4])
		{
			switch (a_fmt) {
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
				std::memcpy(a_out, a_p, 4);
				return true;
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
				a_out[0] = a_p[2]; a_out[1] = a_p[1]; a_out[2] = a_p[0]; a_out[3] = a_p[3];
				return true;
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_TYPELESS: {
				std::uint32_t v;
				std::memcpy(&v, a_p, 4);
				a_out[0] = To8((v & 0x3FF) / 1023.0F); a_out[1] = To8(((v >> 10) & 0x3FF) / 1023.0F);
				a_out[2] = To8(((v >> 20) & 0x3FF) / 1023.0F); a_out[3] = To8(((v >> 30) & 3) / 3.0F);
				return true;
			}
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_TYPELESS: {
				std::uint16_t h[4];
				std::memcpy(h, a_p, 8);
				for (int i = 0; i < 4; ++i) { a_out[i] = To8(HalfToFloat(h[i])); }
				return true;
			}
			default:
				return false;
			}
		}

		std::uint32_t BytesPerPixel(DXGI_FORMAT a_fmt)
		{
			return (a_fmt == DXGI_FORMAT_R16G16B16A16_FLOAT || a_fmt == DXGI_FORMAT_R16G16B16A16_TYPELESS) ? 8 : 4;
		}

		// a 24-bit BMP: a_alpha false -> RGB, true -> the alpha channel as grey
		bool WriteBmp(const std::filesystem::path& a_path, const std::vector<std::uint8_t>& a_rgba, std::uint32_t a_w, std::uint32_t a_h, bool a_alpha)
		{
			const std::uint32_t row = (a_w * 3 + 3) & ~3u;
			const std::uint32_t size = 54 + row * a_h;
			std::vector<std::uint8_t> f(size, 0);
			auto put32 = [&](std::size_t o, std::uint32_t v) { std::memcpy(&f[o], &v, 4); };
			f[0] = 'B'; f[1] = 'M';
			put32(2, size); put32(10, 54); put32(14, 40); put32(18, a_w); put32(22, a_h);
			f[26] = 1; f[28] = 24; put32(34, row * a_h);
			for (std::uint32_t y = 0; y < a_h; ++y) {
				std::uint8_t* dst = &f[54 + (a_h - 1 - y) * row];
				for (std::uint32_t x = 0; x < a_w; ++x) {
					const std::uint8_t* s = &a_rgba[(y * a_w + x) * 4];
					if (a_alpha) { dst[x * 3] = dst[x * 3 + 1] = dst[x * 3 + 2] = s[3]; }
					else { dst[x * 3] = s[2]; dst[x * 3 + 1] = s[1]; dst[x * 3 + 2] = s[0]; }
				}
			}
			std::ofstream out(a_path, std::ios::binary);
			out.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
			return out.good();
		}

		// copies one render target to a staging texture, converts it and writes <stamp>-<name>-colour.bmp / -alpha.bmp
		std::string Dump(RE::RENDER_TARGET a_rt, const char* a_name)
		{
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!renderer) { return std::format("\"{}\":{{\"error\":\"no renderer\"}}", a_name); }
			auto& data = renderer->GetRuntimeData();
			auto* device = reinterpret_cast<ID3D11Device*>(data.forwarder);
			auto* context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
			auto* tex = reinterpret_cast<ID3D11Texture2D*>(data.renderTargets[a_rt].texture);
			if (!device || !context || !tex) { return std::format("\"{}\":{{\"error\":\"no texture\"}}", a_name); }
			D3D11_TEXTURE2D_DESC desc{};
			tex->GetDesc(&desc);
			D3D11_TEXTURE2D_DESC sd = desc;
			sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
			sd.MipLevels = 1; sd.ArraySize = 1; sd.SampleDesc = { 1, 0 };
			ID3D11Texture2D* staging = nullptr;
			if (desc.SampleDesc.Count != 1 || FAILED(device->CreateTexture2D(&sd, nullptr, &staging)) || !staging) {
				return std::format("\"{}\":{{\"error\":\"no staging copy (samples {}, format {})\"}}", a_name, desc.SampleDesc.Count,
					static_cast<int>(desc.Format));
			}
			context->CopySubresourceRegion(staging, 0, 0, 0, 0, tex, 0, nullptr);
			D3D11_MAPPED_SUBRESOURCE m{};
			std::string out;
			if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
				std::vector<std::uint8_t> rgba(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
				bool known = true;
				double lum = 0.0, alpha = 0.0;
				const std::uint32_t bpp = BytesPerPixel(desc.Format);
				for (std::uint32_t y = 0; y < desc.Height && known; ++y) {
					const auto* row = static_cast<const std::uint8_t*>(m.pData) + static_cast<std::size_t>(y) * m.RowPitch;
					for (std::uint32_t x = 0; x < desc.Width; ++x) {
						std::uint8_t* px = &rgba[(static_cast<std::size_t>(y) * desc.Width + x) * 4];
						if (!Pixel(desc.Format, row + static_cast<std::size_t>(x) * bpp, px)) { known = false; break; }
						lum += 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2];
						alpha += px[3];
					}
				}
				context->Unmap(staging, 0);
				const double n = static_cast<double>(desc.Width) * desc.Height;
				if (known) {
					const auto dir = std::filesystem::path("Data/SKSE/Plugins/DragonsEyeMinimap/diag");
					std::filesystem::create_directories(dir);
					const auto colour = dir / std::format("{}-{}-colour.bmp", g_stamp, a_name);
					const auto alphaPath = dir / std::format("{}-{}-alpha.bmp", g_stamp, a_name);
					const bool ok = WriteBmp(colour, rgba, desc.Width, desc.Height, false) && WriteBmp(alphaPath, rgba, desc.Width, desc.Height, true);
					out = std::format("\"{}\":{{\"width\":{},\"height\":{},\"format\":{},\"meanLuminance\":{:.1f},\"meanAlpha\":{:.1f},\"written\":{},"
									  "\"colour\":\"{}\",\"alpha\":\"{}\"}}", a_name, desc.Width, desc.Height, static_cast<int>(desc.Format),
						lum / n, alpha / n, ok ? "true" : "false", colour.generic_string(), alphaPath.generic_string());
				} else {
					out = std::format("\"{}\":{{\"width\":{},\"height\":{},\"format\":{},\"error\":\"format not handled\"}}", a_name, desc.Width,
						desc.Height, static_cast<int>(desc.Format));
				}
			} else {
				out = std::format("\"{}\":{{\"error\":\"Map failed\"}}", a_name);
			}
			staging->Release();
			logger::info("render diagnostics: dumped {}", out);
			return out;
		}

		// what the pass inherits, logged once per change
		void LogState()
		{
			static std::string s_last;
			auto* state = RE::BSGraphics::State::GetSingleton();
			auto* rtm = RE::BSGraphics::RenderTargetManager::GetSingleton();
			if (!state || !rtm) { return; }
			const auto& rd = state->GetRuntimeData();
			const auto& swap = rtm->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];
			const auto& map = rtm->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP];
			const auto& main = rtm->renderTargetData[RE::RENDER_TARGET::kMAIN];
			const std::string now = std::format("dynamic resolution {:.3f} x {:.3f} (previous {:.3f} x {:.3f}); kLOCAL_MAP_SWAP {}x{} format {}, "
												"kLOCAL_MAP {}x{} format {}, kMAIN {}x{} format {}",
				rd.dynamicResolutionWidthRatio, rd.dynamicResolutionHeightRatio, rd.dynamicResolutionPreviousWidthRatio,
				rd.dynamicResolutionPreviousHeightRatio, swap.width, swap.height, static_cast<int>(swap.format.get()), map.width, map.height,
				static_cast<int>(map.format.get()), main.width, main.height, static_cast<int>(main.format.get()));
			if (now != s_last) {
				s_last = now;
				logger::info("render diagnostics: the minimap pass sees {}", now);
			}
		}
#endif
	}

	std::string RequestDump()
	{
#if RUNTIME_LINE == 17
		return "not on this line (SE/AE only for now)";
#else
		g_dumpStage = 1;
		return "both local-map targets are dumped on the next minimap pass - read op=dumpresult";
#endif
	}

	std::string LastDumpJson()
	{
		std::scoped_lock l(g_lock);
		return g_last;
	}

	void AfterGeometry()
	{
#if RUNTIME_LINE != 17
		LogState();
		if (g_dumpStage.load() != 1) { return; }
		const auto t = std::chrono::system_clock::now();
		g_stamp = std::format("{:%Y%m%d-%H%M%S}", std::chrono::floor<std::chrono::seconds>(t));
		g_parts = Dump(RE::RENDER_TARGET::kLOCAL_MAP_SWAP, "swap-before-composite");
		g_dumpStage = 2;
#endif
	}

	void AfterComposite()
	{
#if RUNTIME_LINE != 17
		if (g_dumpStage.load() != 2) { return; }
		const std::string after = Dump(RE::RENDER_TARGET::kLOCAL_MAP, "map-after-composite");
		{
			std::scoped_lock l(g_lock);
			g_last = "{" + g_parts + "," + after + "}";
		}
		g_dumpStage = 0;
#endif
	}
}
