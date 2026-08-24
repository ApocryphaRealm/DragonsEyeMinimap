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

		// How far in from the two edges of that corner the minimap sits, in screen pixels.
		// The corner does the positioning, so this only exists to keep the artwork off the
		// very edge of the screen; set it to 0 to sit flush.
		inline float edgeMargin = 8.0F;

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
		inline std::string controlHideTip = "Hold to control/tap to hide";
		inline std::string controlMoveTip = "Move";
		inline std::string controlZoomTip = "Zoom";
	}

	namespace controls
	{
		// DirectInput scan code of the key that hides/shows the minimap and, held down,
		// controls it. 0 means "whatever the game has bound to Local Map", which is what the
		// mod did before this setting existed.
		inline std::uint32_t hideKeyCode = 0;

		// Tapping this key jumps the map zoom between the two presets below, instead of having
		// to hold the control key and scroll. 0 disables it.
		inline std::uint32_t zoomToggleKeyCode = 0;
		inline float zoomPreset1 = 0.25F;
		inline float zoomPreset2 = 0.75F;

		inline bool followPlayerCameraRotation = true;
		inline float holdDownToControlSecs = 0.15F;
		inline float delayToHideControlsSecs = 1.0F;
	}
}
