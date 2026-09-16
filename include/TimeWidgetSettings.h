#pragma once

// Dragon's Eye Minimap - the TIME WIDGET add-on (1.6.5). A clock, a calendar and a real-world clock
// on the HUD, rebuilt clean-room from A Matter of Time's page and readme (Nexus 12937, SkyAmigo -
// all rights reserved, so none of its scripts, art or ESP is used; its behaviour is the spec).
//
// THE FOMOD CHOICE IS A FILE. The add-on is compiled into DEM's own DLL and is ON only while the
// add-on's settings file exists beside DEM's INI:
//     Data\SKSE\Plugins\DragonsEyeMinimap-TimeWidget.ini
// DEM's FOMOD installs that file when the player ticks "Time Widget"; a player who does not tick it
// has no widget, no page and no change to DEM at all. Everything below is read from and written to
// THAT file (its own [TimeWidget] section), never DEM's main INI, so a plain DEM stays byte-identical.
//
// Every default below is what a fresh tick of the option gives. Key names (INI) are in the comments.

#include <array>
#include <cstdint>
#include <string>

namespace settings::timewidget
{
	// ---- presence (not a setting) -----------------------------------------------------------
	// True once Load() found the add-on file. The widget, its page and its DevBench ops exist
	// only while this is true.
	inline bool present = false;

	// ---- [TimeWidget] master -------------------------------------------------------------------
	inline bool enabled = true;              // bEnabled

	// ---- what to show ---------------------------------------------------------------------------
	inline bool showGameTime = true;         // bShowGameTime   - the in-game clock
	inline bool showGameDate = false;        // bShowGameDate   - the in-game calendar
	inline bool showRealTime = false;        // bShowRealTime   - your real clock
	inline bool showRealDate = false;        // bShowRealDate   - your real calendar
	inline bool showSymbol = true;           // bShowSymbol     - a day/night symbol beside the in-game clock (sun by day, moon by night), drawn by us

	// ---- formats -----------------------------------------------------------------------------------
	// Game time: 0 = 24-hour "HH:MM", 1 = 12-hour "h:MM AM", 2 = 12-hour short "h AM".
	inline std::uint32_t timeFormat = 0;     // uTimeFormat
	// Game date: 0 = lore "Morndas, 17th of Last Seed, 4E 201", 1 = short lore "17th of Last Seed",
	//            2 = "4E 201-08-17", 3 = "17.08.4E201", 4 = day name only "Morndas".
	inline std::uint32_t dateFormat = 0;     // uDateFormat
	// Real time: 0 = 24-hour "HH:MM", 1 = 12-hour "h:MM AM".
	inline std::uint32_t realTimeFormat = 0; // uRealTimeFormat
	// Real date: 0 = "YYYY-MM-DD", 1 = "DD.MM.YYYY", 2 = "Monday, 6 September".
	inline std::uint32_t realDateFormat = 0; // uRealDateFormat
	inline bool showSeconds = false;         // bShowSeconds   - real clock only

	// ---- display -----------------------------------------------------------------------------------
	// Anchor on the 1280x720 HUD stage, then offsets in stage pixels from that corner/edge.
	// 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right, 4 top-centre, 5 bottom-centre.
	inline constexpr int kAnchorCount = 6;
	inline std::uint32_t anchor = 4;         // uAnchor      - top-centre by default (A Matter of Time's own default)
	inline float offsetX = 0.0F;             // fOffsetX
	inline float offsetY = 8.0F;             // fOffsetY
	inline float scale = 1.0F;               // fScale       - 0.25 .. 3.0
	inline std::uint32_t alpha = 100;        // uAlpha       - 0 .. 100 (opacity)
	inline float textSize = 18.0F;           // fTextSize    - stage points, before scale
	inline std::uint32_t textColor = 0xFFFFFF;   // uTextColor - RGB
	// 0 = each shown item on its own line (stacked), 1 = everything on one line, separated by " | ".
	inline std::uint32_t layout = 0;         // uLayout
	inline bool useHudFont = true;           // bUseHudFont  - the game's HUD face; off = the device font

	// ---- control -------------------------------------------------------------------------------------
	// 0 always (as long as no menu is open), 1 timed (shown for fDisplaySeconds after the hotkey),
	// 2 toggle (the hotkey shows/hides), 3 periodic (shown for fDisplaySeconds every fPeriodGameMinutes
	// of game time and/or every fPeriodRealMinutes of real time; the hotkey shows it too).
	inline std::uint32_t mode = 0;           // uMode
	// DirectX scan code of the hotkey; 0 = unbound (the standing rule: every control can be left with
	// no mapping). Default unbound.
	inline std::int32_t hotkeyKeyCode = 0;   // iHotkeyKeyCode
	// Gamepad button mask for the same action; 0 = off (default off, per the controller-binding rule).
	inline std::int32_t hotkeyGamepadButton = 0;   // iHotkeyGamepadButton
	inline float displaySeconds = 5.0F;      // fDisplaySeconds     - timed / periodic
	inline float periodGameMinutes = 60.0F;  // fPeriodGameMinutes  - 0 = off
	inline float periodRealMinutes = 0.0F;   // fPeriodRealMinutes  - 0 = off
	inline bool hideInMenus = true;          // bHideInMenus
	inline bool hideWithMinimap = false;     // bHideWithMinimap    - follow DEM's own hide key

	// ---- the file ----------------------------------------------------------------------------------------
	// The add-on's INI beside DEM's own, resolved from the game's Data folder. Load() sets `present`
	// from its existence and reads every key above (missing keys keep their defaults). Save() writes
	// every key back, keeping comments and unknown keys. RestoreDefaults() only touches the variables.
	const std::string& IniPath();
	void Load();
	bool Save();
	bool Reload();
	void RestoreDefaults();
}
