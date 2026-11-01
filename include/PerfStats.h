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
	enum class Section : int { kFog, kRender, kMarkers, kGpuRender, kCompose, kCacheRender, kGpuCompose, kCellSignature, kCount };

	// What happened to the picture on a frame the camera was updated.
	enum class Outcome : int
	{
		kRedrawn,   // fog rebuild and render ran
		kReused,    // the picture still matched, nothing was drawn
		kHeld,      // ShouldRedrawWorld said no (settle, loading screen, steadiness, iRedrawIntervalMs)
		kComposed,  // an interior's stored picture was drawn into the target instead of the world
		kFogRebuilt,   // the fog overlay was built again
		kFogKept,      // a redraw or compose kept the fog overlay it already had
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
		kCell,          // a fixture attached, detached or a cell finished loading: recapture
		kCellOther,     // a loose item attached or detached: the picture is redrawn, the interior is not captured again
		kObject,        // the interior's signature changed - a static or door was enabled or disabled, or a door opened or closed: recapture
		kDoor,          // the player activated a door in a loaded cell: recapture
		kDoorOther,     // someone else did: recapture
		kDestructible,  // a destruction stage changed, or the player activated something destructible: recapture
		kMenuClosed, kSettings, kFogGrid, kDisplay,
		kCacheReady,    // a capture finished, and the new picture is to be shown
		kOther,
		kCount
	};

	std::int64_t Now();
	void AddSample(Section a_section, std::int64_t a_ticks);
	void Count(Outcome a_outcome);
	void Count(Reason a_reason);
	void Count(EpochCause a_cause);

	// A door activated in a cell that is not loaded (AI travelling elsewhere in the world): counted, changes nothing.
	void CountDoorElsewhere();

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
	void GpuBegin(Section a_section = Section::kGpuRender);
	void GpuEnd();

	// What the interior cache holds now, for op=perf. a_state is a string literal.
	void SetCacheInfo(std::uint64_t a_bytes, std::uint32_t a_tilesX, std::uint32_t a_tilesY, const char* a_state);

	// Everything since the last Reset, and the last closed 10 s window, as JSON. Any thread.
	std::string Json();

	// Zeroes the since-reset numbers, so a test can measure one stretch. Any thread.
	void Reset();
}
