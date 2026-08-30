#pragma once

// COMPASS RING + QUEST POINTER, built into the minimap (design decision, 2026-08-30: "DEM still
// has the compass and should keep it while on AMF - the compass feature should be built in to
// DEM not AMF"). No framework HUD API and no SWF recompile: everything is drawn at RUNTIME into
// an own named clip on the HUD movie root with the ActionScript drawing API - the same technique
// Local Map Upgrade uses for its map border (createEmptyMovieClip at a free depth + lineStyle/
// moveTo/lineTo/curveTo via Invoke). The behaviour is ported from the parked DragonsEyePointers
// reference implementation:
//
//   - The RING appears only while the minimap is deliberately hidden (hide key): a dark backing
//     disc, the ring line, 8 ticks, cardinal letters with N in its own colour - the map's
//     stand-in, riding exactly where the map's inscribed circle sits (stage rect statics).
//   - The QUEST POINTER rides the ring (or the visible map's inscribed circle): the vanilla
//     compass's notched arrowhead in our gold, pointing at the nearest DISPLAYED objective
//     (running quests scanned - PlayerCharacter::objectives measured empty; interior targets
//     resolve to the location's worldLocMarker; root worldspaces compared), with a distance
//     readout (Compass Navigation Overhaul's conventions: 0.01428 units/m, feet by default)
//     and an above/below glyph past +-840 units.
//
// Settings are INI-only in this first build ([Compass] section, read with the mod's own settings
// machinery); the settings-page section follows in the polish pass. Runs from the existing
// HUDMenu::AdvanceMovie hook = the MAIN thread, where Scaleform Invoke is safe.
namespace DEM::compassring
{
	// Called every frame from the AdvanceMovie hook, after Minimap::Advance(). Reads the
	// minimap singleton's stage-rect statics itself; draws nothing until they are valid.
	void Update();

	// Drops the cached clip references (UI reload / new movie). Cheap; safe to call any time.
	void Reset();
}
