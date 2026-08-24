#include "Settings.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"

#include <windows.h>

namespace settings
{
	using namespace utils;

	namespace
	{
		// Section names match the ones baked into the setting names below ("fScale:Display").
		constexpr const char* kDebugSection = "Debug";
		constexpr const char* kDisplaySection = "Display";
		constexpr const char* kControlsSection = "Controls";

		

		std::string iniPath;
		std::string iniFileName;

		// The values the plugin compiles in, captured before the INI is read so that
		// "Restore defaults" means "what you would get with no INI at all".
		struct Defaults
		{
			logger::level logLevel;

			std::uint32_t anchor;
			float edgeMargin;
			float scale;
			std::uint32_t shape;
			bool showOnGameStart;
			std::string controlHideTip;
			std::string controlMoveTip;
			std::string controlZoomTip;

			std::int32_t hideKeyCode;
			std::int32_t zoomToggleKeyCode;
			float zoomPreset1;
			float zoomPreset2;
			bool followPlayerCameraRotation;
			float holdDownToControlSecs;
			float delayToHideControlsSecs;
		};

		Defaults defaults;

		void CaptureDefaults()
		{
			defaults.logLevel = debug::logLevel;

			defaults.anchor = display::anchor;
			defaults.edgeMargin = display::edgeMargin;

			defaults.scale = display::scale;
			defaults.shape = display::shape;
			defaults.showOnGameStart = display::showOnGameStart;
			defaults.controlHideTip = display::controlHideTip;
			defaults.controlMoveTip = display::controlMoveTip;
			defaults.controlZoomTip = display::controlZoomTip;

			defaults.hideKeyCode = controls::hideKeyCode;
			defaults.zoomToggleKeyCode = controls::zoomToggleKeyCode;
			defaults.zoomPreset1 = controls::zoomPreset1;
			defaults.zoomPreset2 = controls::zoomPreset2;
			defaults.followPlayerCameraRotation = controls::followPlayerCameraRotation;
			defaults.holdDownToControlSecs = controls::holdDownToControlSecs;
			defaults.delayToHideControlsSecs = controls::delayToHideControlsSecs;
		}

		// WritePrivateProfileString rewrites a single key in place, so the comments and any
		// keys this plugin does not know about survive a save untouched. It is also what the
		// game's own INISettingCollection::WriteSetting ends up calling, which keeps saving
		// from the menu consistent with the save Minimap::Show()/Hide() already does.
		bool WriteRaw(const char* a_section, const char* a_key, const std::string& a_value)
		{
			if (::WritePrivateProfileStringA(a_section, a_key, a_value.c_str(), iniPath.c_str()))
			{
				return true;
			}

			logger::error("Could not write {}={} to {} (error {})", a_key, a_value, iniPath, ::GetLastError());

			return false;
		}

		bool WriteFloat(const char* a_section, const char* a_key, float a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{:g}", a_value));
		}

		bool WriteUInt(const char* a_section, const char* a_key, std::uint32_t a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{}", a_value));
		}

		bool WriteInt(const char* a_section, const char* a_key, std::int32_t a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{}", a_value));
		}

		// MakeSetting takes the setting's type from the first letter of its name - i signed,
		// u unsigned, f float, b bool, s string - and quietly hands back a setting with a null
		// name when the value passed does not match. The game's collection dereferences that
		// name, so inserting one crashes on startup with nothing useful in the log. Refuse it
		// here instead, where the message can say which setting is at fault.
		void AddChecked(INISettingCollection* a_collection, RE::Setting* a_setting, const char* a_name)
		{
			if (a_setting && a_setting->name)
			{
				a_collection->AddSettings(a_setting);

				return;
			}

			logger::critical("Setting \"{}\" was built with a value that does not match the type its "
							 "name prefix promises, so it has been skipped", a_name);
		}

		bool WriteBool(const char* a_section, const char* a_key, bool a_value)
		{
			return WriteRaw(a_section, a_key, a_value ? "1" : "0");
		}

		// The shipped INI quotes its string values, so keep that convention. A quote typed by
		// the player is dropped rather than escaped, because it would end the value early.
		bool WriteString(const char* a_section, const char* a_key, const std::string& a_value)
		{
			std::string sanitized;
			sanitized.reserve(a_value.size() + 2);

			sanitized.push_back('\"');
			for (char c : a_value)
			{
				if (c != '\"')
				{
					sanitized.push_back(c);
				}
			}
			sanitized.push_back('\"');

			return WriteRaw(a_section, a_key, sanitized);
		}
	}

	namespace
	{
		// Copies whatever the collection currently holds into the variables above. Shared by
		// Init() and Reload() so the two cannot read the INI differently.
		void ReadFromCollection()
		{
			INISettingCollection* iniSettingCollection = INISettingCollection::GetSingleton();

			{
				using namespace debug;
				logLevel = static_cast<logger::level>(iniSettingCollection->GetSetting<std::uint32_t>("uLogLevel:Debug"));
			}

			{
				using namespace display;
				anchor = iniSettingCollection->GetSetting<std::uint32_t>("uAnchor:Display");
				edgeMargin = iniSettingCollection->GetSetting<float>("fEdgeMargin:Display");
				scale = iniSettingCollection->GetSetting<float>("fScale:Display");
				shape = iniSettingCollection->GetSetting<std::uint32_t>("uShape:Display");
				showOnGameStart = iniSettingCollection->GetSetting<bool>("bShowOnGameStart:Display");
				controlHideTip = iniSettingCollection->GetSetting<const char*>("sControlHideTip:Display");
				controlMoveTip = iniSettingCollection->GetSetting<const char*>("sControlMoveTip:Display");
				controlZoomTip = iniSettingCollection->GetSetting<const char*>("sControlZoomTip:Display");
			}

			{
				using namespace controls;
				hideKeyCode = iniSettingCollection->GetSetting<std::int32_t>("iHideKeyCode:Controls");
				zoomToggleKeyCode = iniSettingCollection->GetSetting<std::int32_t>("iZoomToggleKeyCode:Controls");
				zoomPreset1 = iniSettingCollection->GetSetting<float>("fZoomPreset1:Controls");
				zoomPreset2 = iniSettingCollection->GetSetting<float>("fZoomPreset2:Controls");
				followPlayerCameraRotation = iniSettingCollection->GetSetting<bool>("bFollowPlayerCameraRotation:Controls");
				holdDownToControlSecs = iniSettingCollection->GetSetting<float>("fHoldDownToControlSecs:Controls");
				delayToHideControlsSecs = iniSettingCollection->GetSetting<float>("fDelayToHideControlsSecs:Controls");
			}
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		CaptureDefaults();

		iniFileName = a_iniFileName;
		iniPath = std::filesystem::current_path().append("Data\\SKSE\\Plugins").append(a_iniFileName).string();

		INISettingCollection* iniSettingCollection = INISettingCollection::GetSingleton();

		// Registered one at a time through AddChecked, so a type that does not match its name
		// prefix is reported rather than crashing the game as it loads.
		const auto add = [iniSettingCollection](const char* a_name, auto a_value) {
			AddChecked(iniSettingCollection, MakeSetting(a_name, a_value), a_name);
		};

		{
			using namespace debug;
			add("uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));
		}

		{
			using namespace display;
			add("uAnchor:Display", anchor);
			add("fEdgeMargin:Display", edgeMargin);
			add("fScale:Display", scale);
			add("uShape:Display", shape);
			add("bShowOnGameStart:Display", showOnGameStart);
			add("sControlHideTip:Display", controlHideTip.c_str());
			add("sControlMoveTip:Display", controlMoveTip.c_str());
			add("sControlZoomTip:Display", controlZoomTip.c_str());
		}

		{
			using namespace controls;
			add("iHideKeyCode:Controls", static_cast<int>(hideKeyCode));
			add("iZoomToggleKeyCode:Controls", static_cast<int>(zoomToggleKeyCode));
			add("fZoomPreset1:Controls", zoomPreset1);
			add("fZoomPreset2:Controls", zoomPreset2);
			add("bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation);
			add("fHoldDownToControlSecs:Controls", holdDownToControlSecs);
			add("fDelayToHideControlsSecs:Controls", delayToHideControlsSecs);
		}

		if (!iniSettingCollection->ReadFromFile(a_iniFileName))
		{
			logger::warn("Could not read {}, falling back to default options", a_iniFileName);
		}

		ReadFromCollection();
	}

	bool Reload()
	{
		if (iniFileName.empty())
		{
			logger::error("Cannot reload settings before Init() has run");

			return false;
		}

		if (!INISettingCollection::GetSingleton()->ReadFromFile(iniFileName))
		{
			logger::error("Could not re-read {}; keeping the settings already loaded", iniPath);

			return false;
		}

		ReadFromCollection();

		logger::info("Reloaded settings from {}", iniPath);

		return true;
	}

	bool Save()
	{
		if (iniPath.empty())
		{
			logger::error("Cannot save settings before Init() has run");

			return false;
		}

		bool ok = true;

		ok &= WriteUInt(kDebugSection, "uLogLevel", static_cast<std::uint32_t>(debug::logLevel));

		ok &= WriteUInt(kDisplaySection, "uAnchor", display::anchor);
		ok &= WriteFloat(kDisplaySection, "fEdgeMargin", display::edgeMargin);
		ok &= WriteFloat(kDisplaySection, "fScale", display::scale);
		ok &= WriteUInt(kDisplaySection, "uShape", display::shape);
		ok &= WriteBool(kDisplaySection, "bShowOnGameStart", display::showOnGameStart);
		ok &= WriteString(kDisplaySection, "sControlHideTip", display::controlHideTip);
		ok &= WriteString(kDisplaySection, "sControlMoveTip", display::controlMoveTip);
		ok &= WriteString(kDisplaySection, "sControlZoomTip", display::controlZoomTip);

		ok &= WriteInt(kControlsSection, "iHideKeyCode", controls::hideKeyCode);
		ok &= WriteInt(kControlsSection, "iZoomToggleKeyCode", controls::zoomToggleKeyCode);
		ok &= WriteFloat(kControlsSection, "fZoomPreset1", controls::zoomPreset1);
		ok &= WriteFloat(kControlsSection, "fZoomPreset2", controls::zoomPreset2);
		ok &= WriteBool(kControlsSection, "bFollowPlayerCameraRotation", controls::followPlayerCameraRotation);
		ok &= WriteFloat(kControlsSection, "fHoldDownToControlSecs", controls::holdDownToControlSecs);
		ok &= WriteFloat(kControlsSection, "fDelayToHideControlsSecs", controls::delayToHideControlsSecs);

		// Flush the cached INI writes so the file on disk is up to date even if the game is
		// closed the hard way straight afterwards.
		::WritePrivateProfileStringA(nullptr, nullptr, nullptr, iniPath.c_str());

		if (ok)
		{
			logger::info("Saved settings to {}", iniPath);
		}

		return ok;
	}

	void RestoreDefaults()
	{
		debug::logLevel = defaults.logLevel;

		display::anchor = defaults.anchor;
		display::edgeMargin = defaults.edgeMargin;

		display::scale = defaults.scale;
		display::shape = defaults.shape;
		display::showOnGameStart = defaults.showOnGameStart;
		display::controlHideTip = defaults.controlHideTip;
		display::controlMoveTip = defaults.controlMoveTip;
		display::controlZoomTip = defaults.controlZoomTip;

		controls::hideKeyCode = defaults.hideKeyCode;
		controls::zoomToggleKeyCode = defaults.zoomToggleKeyCode;
		controls::zoomPreset1 = defaults.zoomPreset1;
		controls::zoomPreset2 = defaults.zoomPreset2;
		controls::followPlayerCameraRotation = defaults.followPlayerCameraRotation;
		controls::holdDownToControlSecs = defaults.holdDownToControlSecs;
		controls::delayToHideControlsSecs = defaults.delayToHideControlsSecs;
	}

	const std::string& GetIniPath() { return iniPath; }
}
