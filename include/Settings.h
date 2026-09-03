#pragma once

namespace SKSE::log
{
	using level = spdlog::level::level_enum;
}
namespace logger = SKSE::log;

namespace settings
{
	// Reads the INI into the variables below. The values the variables hold when this is
	// called are remembered as the built-in defaults, so RestoreDefaults() can put them back.
	void Init(const std::string& a_iniFileName);

	// Writes every setting below back to the INI that Init() read, leaving the comments and
	// any unrelated keys in that file alone. Returns false if the file could not be written.
	bool Save();

	// Persists just bShowOnGameStart. Minimap::Show()/Hide() change that one setting from
	// outside the settings menu, so they must not drag an unsaved menu edit to disk with it.
	bool SaveShowOnGameStart();

	// Puts every setting back to its built-in default. This only touches the variables;
	// follow it with Save() to persist, and with UI::ApplyLiveSettings() to show it in game.
	void RestoreDefaults();

	// Re-reads the INI that Init() read, discarding any unsaved change made since. Returns
	// false if the file could not be read, leaving the current values alone. Follow it with
	// UI::ApplyLiveSettings() to show the reloaded values in game.
	bool Reload();

	// Full path of the INI Init() read, or an empty string before Init() has run.
	const std::string& GetIniPath();

	namespace debug
	{
		// Info, matching the shipped INI. Trace here would make "Restore defaults" quietly
		// switch the player to logging every frame.
		inline logger::level logLevel = logger::level::trace;
	}

	namespace display
	{
		// Which screen corner the minimap is positioned from.
		enum class Anchor : std::uint32_t
		{
			kTopLeft = 0,
			kTopRight = 1,
			kBottomLeft = 2,
			kBottomRight = 3
		};

		inline constexpr int kAnchorCount = 4;

		inline std::uint32_t anchor = static_cast<std::uint32_t>(Anchor::kTopRight);

		// A nudge per corner, in screen pixels, so switching corners does not carry over the
		// adjustment made to a different one - indexed by Anchor, defaulting to flush (0, 0)
		// on every corner. Positive x is always rightwards and positive y always downwards,
		// whichever corner is anchored, so the two read the same way round no matter which
		// corner is chosen.
		inline std::array<float, kAnchorCount> offsetX = { 0.0F, 0.0F, 0.0F, 0.0F };
		inline std::array<float, kAnchorCount> offsetY = { 0.0F, 0.0F, 0.0F, 0.0F };

		// Guards against an out-of-range uAnchor in a hand-edited INI.
		inline int AnchorIndex()
		{
			return anchor < static_cast<std::uint32_t>(kAnchorCount) ? static_cast<int>(anchor) : 0;
		}

		inline float scale = 0.5F;

		// The scale slider's fixed ends. The upper end is tightened further at runtime, once
		// the artwork has been measured, so the minimap cannot exceed a quarter of the screen.
		inline constexpr float kScaleSliderMin = 0.1F;
		inline constexpr float kScaleSliderMax = 3.0F;
		inline std::uint32_t shape = 0;
		inline bool showOnGameStart = true;
		// Show the location name (and its "cleared" hint) under the map. ON by default, since
		// that is how the mod has always looked.
		//
		// A compatibility switch for non-English games (request, 2026-09-02). The title is drawn
		// with the interface font the game supplies; in localisations whose glyphs that font does
		// not carry, or whose names are longer than the space allows, the result can be missing
		// characters or text running past the frame. There is nothing the mod can do about
		// another language's font, so the honest remedy is to let those players switch the title
		// off and keep the map.
		inline bool showLocationName = true;   // bShowLocationName:Display

		// MINIMAP FRAME THEMES. sTheme:Display is the file name (stem) of a SWF in
		// Data/Interface/DragonsEyeMinimapThemes/; empty means the built-in frame.
		//
		// A theme REPLACES the frame artwork - it does not recolour it. The 1.6.0 system carried
		// a colour per theme and applied it as a multiply tint, so a "theme" could only ever be a
		// recolour of the one frame; corrected by the project owner on 2026-09-02. Themes live
		// under Data/Interface because that is where Scaleform resolves loadMovie paths from.
		inline std::string theme;
	}

	namespace controls
	{
		// DirectInput scan code of a key that shows or hides the minimap the moment it is
		// pressed. 0 disables it. This is the only way to hide the minimap - there is no
		// tap-to-hide on the map-control binding any more. Signed, because the INI name
		// starts with "i" and the setting type is taken from that prefix.
		//
		// Ships bound to K (0x25). These used to ship at 0, which made both features invisible:
		// nothing hinted that hide/show or the zoom toggle existed unless the player went looking
		// in the settings menu. the author asked for every mod to ship with its keys already assigned.
		//
		// Note these are DirectInput scan codes, not ASCII or virtual-key codes - K is 0x25 and
		// L is 0x26. Using the wrong table binds some unrelated key rather than failing visibly.
		inline std::int32_t hideKeyCode = 0x25;

		// Hold the hide key at least this long to PAN instead of toggling (1.5.9, the author: "hold to
		// pan and press to hide using the same button"). While held: mouse moves the map, the
		// wheel zooms it, and the camera ignores both. A shorter press still toggles hide/show.
		inline float holdToPanSecs = 0.25F;

		// Controller equivalent of the hide key (XInput button mask): tap = hide/show, hold =
		// pan with the RIGHT stick. Default LEFT BUMPER (0x0100) - the author, 2026-09-01:
		// holding a shoulder button while steering the right stick is natural, holding the
		// stick's own click (the old R3 default) is not. 0 disables.
		inline std::int32_t panHoldGamepadButton = 0x0100;

		// The controller button is OFF unless switched on (design decision, 2026-08-30): every gamepad button is
		// already used for something, and PC players remap through Steam Input anyway.
		inline bool gamepadHideButtonEnabled = false;

		// Tapping this key jumps the map zoom between the two presets below, instead of having
		// to hold the control key and scroll. 0 disables it. Ships bound to L (0x26).
		inline std::int32_t zoomToggleKeyCode = 0x26;

		// The two zoom levels the toggle key alternates between - not "whichever is further
		// from where the camera happens to be", which behaves oddly once the player has
		// manually scrolled the map to a third value. Which of the two is currently active is
		// tracked at runtime by Minimap, not here, since it is not something to save.
		inline float zoomDefault = 0.0F;
		inline float zoomZoomedIn = 1.0F;

		inline bool followPlayerCameraRotation = true;

	}

	// WHEN THE WORLD IMAGE UNDER THE MINIMAP IS REDRAWN.
	//
	// The minimap is not an overlay: the picture inside the frame is the world, drawn through the
	// game's own local-map path (WorldRendering.cpp). That draw runs the engine's cull jobs over
	// the live scene graph, and to do it it briefly unhides the object-LOD root, turns node fade
	// off and clears terrain render passes - global state that belongs to the frame the player is
	// looking at. Doing that WHILE the engine is streaming a new cell in is asking for trouble,
	// and a report of trees rendering deformed straight after coc/cow (2026-09-02) is exactly the
	// shape of trouble it would cause.
	//
	// There is no way to draw a live local map without this path - it is the same code the
	// vanilla local map uses - so the answer is to not run it at the moments it can do harm, and
	// to let anyone who suspects it turn the rate right down.
	namespace rendering
	{
		// Hold the redraw off while the world is not steady: during a loading screen, and for
		// iSettleMs after the player's cell or worldspace changes. The map keeps showing the
		// last picture it drew for that fraction of a second, which is what the vanilla local
		// map does anyway. ON by default - this is the safeguard, not an option to tune.
		inline bool skipWhileWorldSettles = true;   // bSkipWhileWorldSettles:Rendering

		// How long after a cell or worldspace change counts as "still settling", in
		// milliseconds. Long enough that the objects have attached, short enough not to be
		// noticed: a coc lands the player in a loading screen anyway.
		inline std::int32_t settleMs = 1500;        // iSettleMs:Rendering

		// Minimum gap between redraws, in milliseconds. 0 means redraw every frame, which is
		// how the minimap has always worked and what makes it track the player smoothly. Raise
		// it to trade smoothness for a world image that is touched far less often - at 250 the
		// engine's scene graph is walked four times a second instead of sixty, and the map
		// steps rather than glides. Nothing else about the minimap changes: the frame, the
		// markers, the compass ring and the player arrow all still update every frame.
		inline std::int32_t redrawIntervalMs = 0;   // iRedrawIntervalMs:Rendering
	}

	// Built-in compass ring + quest pointer (design decision, 2026-08-30) - INI-only for now
	// ([Compass]); every piece individually toggleable, and the ring only exists while the
	// minimap is deliberately hidden, so the defaults can ship ON.
	namespace compass
	{
		inline bool compassRing = true;        // bCompassRing:Compass
		inline bool questPointer = true;       // bQuestPointer:Compass
		inline bool metricUnits = false;       // bMetricUnits:Compass - feet by default, like the vanilla compass
		// Stage-pixel conversions of the old separate widget's SCREEN-pixel defaults (the HUD
		// movie scales ~2.5x onto a 3200-wide screen), so the built-in compass keeps its look.
		inline float ringGap = 2.4F;           // fRingGap:Compass - inward of the map frame's inscribed circle
		inline float ringThickness = 0.8F;     // fRingThickness:Compass
		inline float pointerSize = 8.0F;       // fPointerSize:Compass
		inline float labelSize = 8.0F;         // fLabelSize:Compass
		inline std::uint32_t ringColor = 0xE0E0E0;    // uRingColor:Compass (RGB)
		inline std::uint32_t northColor = 0xFF3030;   // uNorthColor:Compass
		inline std::uint32_t pointerColor = 0xFFE040; // uPointerColor:Compass - our gold
		inline std::uint32_t discAlpha = 90;   // uDiscAlpha:Compass - backing disc opacity 0-100
	}
}
