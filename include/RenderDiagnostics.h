#pragma once

// RENDER DIAGNOSTICS (1.7.5, the black ground under Community Shaders' Upscaling - tests f6ac / 67ea / c5f9).
//
// With CS Upscaling loaded the minimap's ground comes out dark, whatever its upscaling method or frame generation, and
// with the feature unloaded it draws. Two questions decide where a fix belongs, and both are answered here, from the
// render targets themselves (no screen capture):
//   1. what DEM's off-screen pass inherits - the game's dynamic-resolution ratios (BSGraphics::State) and the sizes of
//      the local-map targets - logged once per change, as the pass sees them;
//   2. WHERE the ground goes dark - dem.control op=dump saves kLOCAL_MAP_SWAP right after the geometry pass (before the
//      ISLocalMap composite) and kLOCAL_MAP after it, colour and alpha, as BMP files the log names.
// Nothing changes for players: the dump happens only when DevBench asks, and the logging is one line per change.
// SE/AE (the CommonLibSSE-NG line) only for now; the 1.7 line reports "not on this line".

#include <string>

namespace rdiag
{
	// DevBench: dump both targets on the next minimap pass. Returns the request's note.
	std::string RequestDump();

	// The last dump's result as JSON ({} until one ran).
	std::string LastDumpJson();

	// RenderOffScreen: after the geometry pass (kLOCAL_MAP_SWAP) and after the composite (kLOCAL_MAP).
	void AfterGeometry();
	void AfterComposite();
}
