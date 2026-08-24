#pragma once

namespace UI
{
	// Adds this mod's page to the SKSE Menu Framework's Mod Control Panel. Safe to call when
	// the framework is missing or too old to drive: it logs why and does nothing else.
	// Must be called once all SKSE plugins have been loaded (kDataLoaded is a good moment),
	// because it looks the framework up as an already-loaded module.
	void Register();

	// Pushes the current values of settings::display and settings::debug into the running
	// game: the Scaleform minimap clip, the local map shape and the log level. Settings that
	// are read every frame anyway (the settings::controls ones) need nothing doing.
	//
	// Safe to call from any thread and at any point in the load order; the parts that touch
	// Scaleform are queued onto the main thread and skipped while the minimap does not exist.
	void ApplyLiveSettings();

	namespace SettingsPanel
	{
		void __stdcall Render();
	}
}
