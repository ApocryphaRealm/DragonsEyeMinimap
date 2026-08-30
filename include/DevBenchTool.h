#pragma once

// dem.control - DEM's own DevBench driving tool (the project's driving-tool standard: every mod
// ships a surface Claude can push, not just observe). First need, 2026-08-30: the compass test -
// spliced hide-key presses were not reaching the MenuControls handler on the Test Build, and a
// test that cannot toggle the minimap headlessly cannot photograph the ring. The tool drives the
// C++ methods directly, marshalled to the main thread, so it works regardless of input routing.
namespace DEM::devbench
{
	// Register with DevBench if present. Call at kPostLoad (first try) and kDataLoaded (last
	// attempt - logs "not installed" once when still absent). Safe to call repeatedly.
	void Init(bool a_lastAttempt);
}
