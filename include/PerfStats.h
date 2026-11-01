#pragma once

// RENDER MEASUREMENT (1.8.0). What the minimap's per-frame work costs, and how often the world picture is really redrawn.
//
// Timers are QueryPerformanceCounter around the fog rebuild, the off-screen render and the marker block. With
// [Performance] bDiagnostics on, D3D11 timestamp queries also bracket the off-screen render; they are read back with
// D3D11_ASYNC_GETDATA_DONOTFLUSH, a result that is not ready yet is simply looked at again on a later frame, and nothing
// ever waits on the GPU. Every 10 s one line goes to the log (debug level, or info with bDiagnostics on);
// dem.control op=perf hands the same numbers to a test without parsing a log.
//
// Advance() and PreRender() can run on different threads and DevBench reads from its own, so everything here is atomic.

#include <cstdint>
#include <string>

namespace perf
{
	enum class Section : int { kFog, kRender, kMarkers, kGpuRender, kCount };

	// What happened to the picture on a frame the camera was updated.
	enum class Outcome : int
	{
		kRedrawn,   // fog rebuild and render ran
		kReused,    // the picture still matched, nothing was drawn
		kHeld,      // ShouldRedrawWorld said no (settle, loading screen, steadiness, iRedrawIntervalMs)
		kCount
	};

	// Why a frame had to redraw instead of reusing: the first test that failed.
	enum class Reason : int
	{
		kReuseOff, kFirst, kEpoch, kAge, kGap, kWorld, kShapeStyle, kZoomBounds, kPosition, kRotation, kCount
	};

	// What raised the content epoch.
	enum class EpochCause : int
	{
		kCell, kDoor, kDestructible, kMenuClosed, kSettings, kFogGrid, kDisplay, kOther, kCount
	};

	std::int64_t Now();
	void AddSample(Section a_section, std::int64_t a_ticks);
	void Count(Outcome a_outcome);
	void Count(Reason a_reason);
	void Count(EpochCause a_cause);

	class ScopedTimer
	{
	public:
		explicit ScopedTimer(Section a_section) : section(a_section), start(Now()) {}
		~ScopedTimer() { AddSample(section, Now() - start); }
		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;

	private:
		Section section;
		std::int64_t start;
	};

	// Once per pass from PreRender: reads finished GPU results and closes the 10 s window. Render thread.
	void Tick();

	// Bracket the off-screen render. No-ops unless [Performance] bDiagnostics is on. Render thread.
	void GpuBegin();
	void GpuEnd();

	// Everything since the last Reset, and the last closed 10 s window, as JSON. Any thread.
	std::string Json();

	// Zeroes the since-reset numbers, so a test can measure one stretch. Any thread.
	void Reset();
}
