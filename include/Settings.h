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
		// These are the values a fresh install starts from, and the ones "Restore defaults"
		// returns to. They are the author's preferred layout rather than the mod's original
		// 0.65 / 0.2 / 1.0.
		inline float positionX = 0.86F;
		inline float positionY = 0.04F;
		inline float scale = 0.5F;
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

		inline bool followPlayerCameraRotation = true;
		inline float holdDownToControlSecs = 0.15F;
		inline float delayToHideControlsSecs = 1.0F;
	}
}
