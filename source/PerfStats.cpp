#include "PerfStats.h"

#include "Settings.h"

#include "RE/R/Renderer.h"

#include <array>
#include <atomic>
#include <format>
#include <mutex>

#include <d3d11.h>

namespace perf
{
	namespace
	{
		constexpr double kWindowSeconds = 10.0;

		constexpr std::array<const char*, static_cast<int>(Section::kCount)> kSectionNames = { "fog", "render", "markers", "gpuRender" };
		constexpr std::array<const char*, static_cast<int>(Reason::kCount)> kReasonNames = {
			"reuseOff", "first", "epoch", "age", "gap", "world", "shapeStyle", "zoomBounds", "position", "rotation"
		};
		constexpr std::array<const char*, static_cast<int>(EpochCause::kCount)> kCauseNames = {
			"cell", "door", "destructible", "menuClosed", "settings", "fogGrid", "display", "other"
		};

		struct Stat
		{
			std::atomic<std::int64_t> ticks{ 0 };
			std::atomic<std::int64_t> maxTicks{ 0 };
			std::atomic<std::uint64_t> samples{ 0 };

			void Add(std::int64_t a_ticks)
			{
				ticks.fetch_add(a_ticks, std::memory_order_relaxed);
				samples.fetch_add(1, std::memory_order_relaxed);
				std::int64_t seen = maxTicks.load(std::memory_order_relaxed);
				while (a_ticks > seen && !maxTicks.compare_exchange_weak(seen, a_ticks, std::memory_order_relaxed)) {}
			}
		};

		// One scope of numbers: the open 10 s window, and everything since the last Reset.
		struct Scope
		{
			std::array<Stat, static_cast<int>(Section::kCount)> sections;
			std::array<std::atomic<std::uint64_t>, static_cast<int>(Outcome::kCount)> outcomes{};
			std::array<std::atomic<std::uint64_t>, static_cast<int>(Reason::kCount)> reasons{};
			std::array<std::atomic<std::uint64_t>, static_cast<int>(EpochCause::kCount)> causes{};
		};

		Scope g_window;
		Scope g_total;

		std::mutex g_lastLock;
		std::string g_lastWindowJson = "{}";

		std::atomic<std::int64_t> g_windowStart{ 0 };
		std::atomic<std::int64_t> g_totalStart{ 0 };

		std::int64_t Frequency()
		{
			static const std::int64_t freq = [] {
				LARGE_INTEGER f{};
				QueryPerformanceFrequency(&f);
				return f.QuadPart > 0 ? static_cast<std::int64_t>(f.QuadPart) : std::int64_t{ 1 };
			}();
			return freq;
		}

		double ToMs(std::int64_t a_ticks) { return static_cast<double>(a_ticks) * 1000.0 / static_cast<double>(Frequency()); }

		std::string ScopeJson(Scope& a_scope, double a_seconds)
		{
			std::string out = std::format("{{\"seconds\":{:.1f},\"redraws\":{},\"reused\":{},\"held\":{}", a_seconds,
				a_scope.outcomes[static_cast<int>(Outcome::kRedrawn)].load(), a_scope.outcomes[static_cast<int>(Outcome::kReused)].load(),
				a_scope.outcomes[static_cast<int>(Outcome::kHeld)].load());
			for (int i = 0; i < static_cast<int>(Section::kCount); ++i) {
				const Stat& s = a_scope.sections[i];
				const std::uint64_t n = s.samples.load();
				out += std::format(",\"{}\":{{\"n\":{},\"avgMs\":{:.3f},\"maxMs\":{:.3f}}}", kSectionNames[i], n,
					n ? ToMs(s.ticks.load()) / static_cast<double>(n) : 0.0, ToMs(s.maxTicks.load()));
			}
			out += ",\"redrawReasons\":{";
			for (int i = 0; i < static_cast<int>(Reason::kCount); ++i) {
				out += std::format("{}\"{}\":{}", i ? "," : "", kReasonNames[i], a_scope.reasons[i].load());
			}
			out += "},\"epochBumps\":{";
			for (int i = 0; i < static_cast<int>(EpochCause::kCount); ++i) {
				out += std::format("{}\"{}\":{}", i ? "," : "", kCauseNames[i], a_scope.causes[i].load());
			}
			out += "}}";
			return out;
		}

		void Zero(Scope& a_scope)
		{
			for (Stat& s : a_scope.sections) {
				s.ticks.store(0);
				s.maxTicks.store(0);
				s.samples.store(0);
			}
			for (auto& v : a_scope.outcomes) { v.store(0); }
			for (auto& v : a_scope.reasons) { v.store(0); }
			for (auto& v : a_scope.causes) { v.store(0); }
		}

		std::string LogLine(Scope& a_scope, double a_seconds)
		{
			std::string out = std::format("Performance ({:.1f} s): redrawn {}, reused {}, held {}", a_seconds,
				a_scope.outcomes[static_cast<int>(Outcome::kRedrawn)].load(), a_scope.outcomes[static_cast<int>(Outcome::kReused)].load(),
				a_scope.outcomes[static_cast<int>(Outcome::kHeld)].load());
			for (int i = 0; i < static_cast<int>(Section::kCount); ++i) {
				const Stat& s = a_scope.sections[i];
				const std::uint64_t n = s.samples.load();
				if (n) {
					out += std::format(" | {} avg {:.3f} ms, max {:.3f} ms ({})", kSectionNames[i], ToMs(s.ticks.load()) / static_cast<double>(n),
						ToMs(s.maxTicks.load()), n);
				}
			}
			out += " | redraw reasons:";
			for (int i = 0; i < static_cast<int>(Reason::kCount); ++i) {
				if (const auto v = a_scope.reasons[i].load()) { out += std::format(" {} {}", kReasonNames[i], v); }
			}
			out += " | epoch bumps:";
			for (int i = 0; i < static_cast<int>(EpochCause::kCount); ++i) {
				if (const auto v = a_scope.causes[i].load()) { out += std::format(" {} {}", kCauseNames[i], v); }
			}
			return out;
		}

		// ---- GPU timing -------------------------------------------------------------------------------------------
		// A small ring of query sets, because results arrive some frames after the draw. Render thread only, except
		// g_gpuState, which DevBench reads. When every set is still waiting a redraw just goes unmeasured.
		enum GpuState : int { kGpuOff, kGpuOn, kGpuUnavailable };
		std::atomic<int> g_gpuState{ kGpuOff };
		std::atomic<std::uint64_t> g_gpuUnmeasured{ 0 };

		struct QuerySet
		{
			ID3D11Query* disjoint = nullptr;
			ID3D11Query* begin = nullptr;
			ID3D11Query* end = nullptr;
			bool waiting = false;
		};
		constexpr int kQuerySets = 6;
		std::array<QuerySet, kQuerySets> g_sets;
		bool g_setsCreated = false;
		int g_open = -1;

		ID3D11Device* Device()
		{
#if RUNTIME_LINE == 17
			auto* data = RE::BSGraphics::Renderer::GetRendererDataSingleton();
			return data ? reinterpret_cast<ID3D11Device*>(data->forwarder) : nullptr;
#else
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			return renderer ? reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder) : nullptr;
#endif
		}

		ID3D11DeviceContext* Context()
		{
#if RUNTIME_LINE == 17
			auto* data = RE::BSGraphics::Renderer::GetRendererDataSingleton();
			return data ? reinterpret_cast<ID3D11DeviceContext*>(data->context) : nullptr;
#else
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			return renderer ? reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context) : nullptr;
#endif
		}

		bool CreateSets()
		{
			if (g_setsCreated) { return true; }
			if (g_gpuState.load() == kGpuUnavailable) { return false; }

			ID3D11Device* device = Device();
			if (!device) { return false; }   // not ready yet (rule 17): asked again on the next redraw

			D3D11_QUERY_DESC disjointDesc{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
			D3D11_QUERY_DESC stampDesc{ D3D11_QUERY_TIMESTAMP, 0 };
			for (QuerySet& set : g_sets) {
				if (FAILED(device->CreateQuery(&disjointDesc, &set.disjoint)) || FAILED(device->CreateQuery(&stampDesc, &set.begin)) ||
					FAILED(device->CreateQuery(&stampDesc, &set.end))) {
					logger::warn("Performance: the device refused a timestamp query; GPU timing stays off this session");
					g_gpuState.store(kGpuUnavailable);
					for (QuerySet& s : g_sets) {
						if (s.disjoint) { s.disjoint->Release(); s.disjoint = nullptr; }
						if (s.begin) { s.begin->Release(); s.begin = nullptr; }
						if (s.end) { s.end->Release(); s.end = nullptr; }
					}
					return false;
				}
			}
			g_setsCreated = true;
			g_gpuState.store(kGpuOn);
			logger::debug("Performance: {} GPU timestamp query sets created", kQuerySets);
			return true;
		}

		void PollGpu()
		{
			if (!g_setsCreated) { return; }
			ID3D11DeviceContext* context = Context();
			if (!context) { return; }

			for (QuerySet& set : g_sets) {
				if (!set.waiting) { continue; }

				D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dis{};
				UINT64 t0 = 0, t1 = 0;
				// DONOTFLUSH: never pushes work at the GPU and never waits. S_FALSE means "look again on a later frame".
				if (context->GetData(set.disjoint, &dis, sizeof(dis), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
					context->GetData(set.begin, &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
					context->GetData(set.end, &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
					continue;
				}

				set.waiting = false;
				if (!dis.Disjoint && dis.Frequency > 0 && t1 >= t0) {
					const double seconds = static_cast<double>(t1 - t0) / static_cast<double>(dis.Frequency);
					AddSample(Section::kGpuRender, static_cast<std::int64_t>(seconds * static_cast<double>(Frequency())));
				}
			}
		}
	}

	std::int64_t Now()
	{
		LARGE_INTEGER c{};
		QueryPerformanceCounter(&c);
		return c.QuadPart;
	}

	void AddSample(Section a_section, std::int64_t a_ticks)
	{
		const int i = static_cast<int>(a_section);
		g_window.sections[i].Add(a_ticks);
		g_total.sections[i].Add(a_ticks);
	}

	void Count(Outcome a_outcome)
	{
		const int i = static_cast<int>(a_outcome);
		g_window.outcomes[i].fetch_add(1, std::memory_order_relaxed);
		g_total.outcomes[i].fetch_add(1, std::memory_order_relaxed);
	}

	void Count(Reason a_reason)
	{
		const int i = static_cast<int>(a_reason);
		g_window.reasons[i].fetch_add(1, std::memory_order_relaxed);
		g_total.reasons[i].fetch_add(1, std::memory_order_relaxed);
	}

	void Count(EpochCause a_cause)
	{
		const int i = static_cast<int>(a_cause);
		g_window.causes[i].fetch_add(1, std::memory_order_relaxed);
		g_total.causes[i].fetch_add(1, std::memory_order_relaxed);
	}

	void Tick()
	{
		PollGpu();

		const std::int64_t now = Now();
		std::int64_t start = g_windowStart.load();
		if (start == 0) {
			g_windowStart.store(now);
			std::int64_t expected = 0;
			g_totalStart.compare_exchange_strong(expected, now);
			return;
		}

		const double seconds = static_cast<double>(now - start) / static_cast<double>(Frequency());
		if (seconds < kWindowSeconds) { return; }

		// Samples landing from the HUD thread during this copy fall in either window; a few in 10 s is not a measurement error
		// anyone can see, and a lock on the per-frame path would cost more than it saved.
		const std::string line = LogLine(g_window, seconds);
		if (settings::performance::diagnostics) { logger::info("{}", line); }
		else { logger::debug("{}", line); }

		{
			std::string json = ScopeJson(g_window, seconds);
			std::scoped_lock lock(g_lastLock);
			g_lastWindowJson = std::move(json);
		}
		Zero(g_window);
		g_windowStart.store(now);
	}

	void GpuBegin()
	{
		g_open = -1;
		if (!settings::performance::diagnostics || !CreateSets()) { return; }

		ID3D11DeviceContext* context = Context();
		if (!context) { return; }

		for (int i = 0; i < kQuerySets; ++i) {
			if (!g_sets[i].waiting) {
				context->Begin(g_sets[i].disjoint);
				context->End(g_sets[i].begin);
				g_open = i;
				return;
			}
		}
		g_gpuUnmeasured.fetch_add(1, std::memory_order_relaxed);
	}

	void GpuEnd()
	{
		if (g_open < 0) { return; }

		ID3D11DeviceContext* context = Context();
		if (context) {
			context->End(g_sets[g_open].end);
			context->End(g_sets[g_open].disjoint);
			g_sets[g_open].waiting = true;
		}
		g_open = -1;
	}

	std::string Json()
	{
		const std::int64_t now = Now();
		const std::int64_t totalStart = g_totalStart.load();
		const double totalSeconds = totalStart ? static_cast<double>(now - totalStart) / static_cast<double>(Frequency()) : 0.0;

		std::string lastWindow;
		{
			std::scoped_lock lock(g_lastLock);
			lastWindow = g_lastWindowJson;
		}

		const char* gpu = "off";
		if (g_gpuState.load() == kGpuOn) { gpu = settings::performance::diagnostics ? "on" : "off"; }
		else if (g_gpuState.load() == kGpuUnavailable) { gpu = "unavailable"; }

		return std::format("{{\"ok\":true,\"op\":\"perf\",\"reusePicture\":{},\"maxPictureAgeMs\":{},\"diagnostics\":{},\"redrawIntervalMs\":{},"
						   "\"gpuTiming\":\"{}\",\"gpuUnmeasured\":{},\"sinceReset\":{},\"lastWindow\":{}}}",
			settings::performance::reusePicture, settings::performance::maxPictureAgeMs, settings::performance::diagnostics,
			settings::rendering::redrawIntervalMs, gpu, g_gpuUnmeasured.load(), ScopeJson(g_total, totalSeconds), lastWindow);
	}

	void Reset()
	{
		Zero(g_total);
		g_totalStart.store(Now());
		g_gpuUnmeasured.store(0);
		logger::debug("Performance: since-reset numbers cleared");
	}
}
