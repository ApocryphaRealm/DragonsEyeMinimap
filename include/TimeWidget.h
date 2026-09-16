#pragma once

// Dragon's Eye Minimap - the TIME WIDGET add-on's module (1.6.5). See TimeWidgetSettings.h for what it
// is and why it is a FOMOD-installed file. This header is the CONTRACT between the three owners of the
// add-on: TimeWidget.cpp (this module), TimeWidgetUI.cpp (the settings page) and DevBenchTool.cpp (the
// dem.control "time" ops). Nothing here runs while settings::timewidget::present is false.
//
// Drawing: the same pattern as CompassRing.cpp - runtime-created TextFields inside one movie clip in
// the HUD movie, the HUD's own typeface, positioned in the 1280x720 stage. Created once when the HUD
// menu is accepted, updated from the AdvanceMovie hook with a one-second throttle (a clock is the one
// inherently continuous feature; everything else is event-driven). No engine hook of its own.
//
// Time sources: RE::Calendar for the in-game clock/calendar (GetHour, GetDay, GetMonth, GetYear,
// GetDayOfWeek, GetMonthName/GetDayName - the game's own names, so translations follow the game);
// std::chrono::system_clock + localtime for the real clock/calendar. The day-night symbol is drawn with
// the Scaleform drawing API (beginFill/lineStyle, as the compass ring does) - a sun disc from 6:00 to
// 18:00 game time, a crescent otherwise. Our own art; nothing of A Matter of Time's.

#include <string>

namespace RE { class HUDMenu; }

namespace timewidget
{
	// Reads the add-on INI (settings::timewidget::Load) and, when present, installs the hotkey input
	// sink. Call at kDataLoaded, after settings::Init. Safe to call when the file is absent - it then
	// does nothing and logs one line saying the add-on is not installed.
	void Init();

	// The HUD movie exists: create the clip and its fields. Called from the HUDMenu::Accept hook, after
	// the minimap's own creation. Idempotent.
	void OnHudAccepted(RE::HUDMenu* a_hudMenu);

	// Every frame from the HUDMenu::AdvanceMovie hook. Throttles its own text update to once per real
	// second; applies the control mode (always / timed / toggle / periodic), the menu-open rule and the
	// hide-with-minimap rule to visibility every frame (cheap: one _visible member).
	void Advance(RE::HUDMenu* a_hudMenu);

	// Re-applies position, scale, alpha, text size, colour, layout and font after any setting changed
	// (the page calls it after every slider; DevBench after time.set). Main thread.
	void ApplyLayout();

	// The hotkey's action for the current mode: timed -> show for fDisplaySeconds; toggle -> flip;
	// periodic -> show now; always -> nothing. Also what dem.control time.trigger calls.
	void Trigger();

	// The strings the widget is showing right now (empty when that item is off) - for the page's live
	// readout and for the proof (compared against the console's own GetCurrentTime / GameDay readings).
	std::string GameTimeText();
	std::string GameDateText();
	std::string RealTimeText();
	std::string RealDateText();
	bool Visible();   // what the last Advance decided

	// present, enabled, mode, visible, the four strings, the period timers, the hotkey state.
	std::string StatusJson();
}
