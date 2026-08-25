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
	}

	namespace controls
	{
		// DirectInput scan code of a key that shows or hides the minimap the moment it is
		// pressed. 0 disables it. This is the only way to hide the minimap - there is no
		// tap-to-hide on the map-control binding any more. Signed, because the INI name
		// starts with "i" and the setting type is taken from that prefix.
		inline std::int32_t hideKeyCode = 0;

		// Tapping this key jumps the map zoom between the two presets below, instead of having
		// to hold the control key and scroll. 0 disables it.
		inline std::int32_t zoomToggleKeyCode = 0;

		// The two zoom levels the toggle key alternates between - not "whichever is further
		// from where the camera happens to be", which behaves oddly once the player has
		// manually scrolled the map to a third value. Which of the two is currently active is
		// tracked at runtime by Minimap, not here, since it is not something to save.
		inline float zoomDefault = 0.25F;
		inline float zoomZoomedIn = 0.75F;

		inline bool followPlayerCameraRotation = true;
	}
}
