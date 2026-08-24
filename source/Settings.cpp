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
			float offsetX;
			float offsetY;
			float scale;
			std::uint32_t shape;
			bool showOnGameStart;

			std::int32_t hideKeyCode;
			std::int32_t zoomToggleKeyCode;
			float zoomDefault;
			float zoomZoomedIn;
			bool followPlayerCameraRotation;
		};

		Defaults defaults;

		void CaptureDefaults()
		{
			defaults.logLevel = debug::logLevel;

			defaults.anchor = display::anchor;
			defaults.offsetX = display::offsetX;
			defaults.offsetY = display::offsetY;

			defaults.scale = display::scale;
			defaults.shape = display::shape;
			defaults.showOnGameStart = display::showOnGameStart;

			defaults.hideKeyCode = controls::hideKeyCode;
			defaults.zoomToggleKeyCode = controls::zoomToggleKeyCode;
			defaults.zoomDefault = controls::zoomDefault;
			defaults.zoomZoomedIn = controls::zoomZoomedIn;
			defaults.followPlayerCameraRotation = controls::followPlayerCameraRotation;
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

		// RE::INISettingCollection::GetSetting returns null for a name that is not in the
		// collection, and the templated GetSetting<T> helpers dereference that without
		// checking. AddChecked below deliberately skips a malformed setting, so a skipped one
		// would then be read back as null and crash during SKSEPluginLoad - trading one fatal
		// bug for another. Read through here instead: the value keeps whatever default it
		// already had, and the log says which setting went missing.
		template <typename T>
		T Read(INISettingCollection* a_collection, const char* a_name, T a_fallback)
		{
			if (!a_collection->GetSetting(a_name))
			{
				logger::error("Setting \"{}\" is missing from the collection; keeping the current value", a_name);

				return a_fallback;
			}

			return a_collection->GetSetting<T>(a_name);
		}

		std::string ReadString(INISettingCollection* a_collection, const char* a_name, const std::string& a_fallback)
		{
			auto* setting = a_collection->GetSetting(a_name);
			if (!setting || !setting->GetString())
			{
				logger::error("Setting \"{}\" is missing or empty; keeping the current value", a_name);

				return a_fallback;
			}

			return setting->GetString();
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
			INISettingCollection* c = INISettingCollection::GetSingleton();

			{
				using namespace debug;
				const auto raw = Read<std::uint32_t>(c, "uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));

				// spdlog indexes its level table by this value, so a hand-edited uLogLevel=99
				// would read off the end of it the next time anything logged.
				logLevel = raw <= static_cast<std::uint32_t>(logger::level::off)
							   ? static_cast<logger::level>(raw)
							   : logger::level::info;
			}

			{
				using namespace display;
				anchor = Read<std::uint32_t>(c, "uAnchor:Display", anchor);
				offsetX = Read<float>(c, "fOffsetX:Display", offsetX);
				offsetY = Read<float>(c, "fOffsetY:Display", offsetY);
				scale = Read<float>(c, "fScale:Display", scale);
				shape = Read<std::uint32_t>(c, "uShape:Display", shape);
				showOnGameStart = Read<bool>(c, "bShowOnGameStart:Display", showOnGameStart);
			}

			{
				using namespace controls;
				hideKeyCode = Read<std::int32_t>(c, "iHideKeyCode:Controls", hideKeyCode);
				zoomToggleKeyCode = Read<std::int32_t>(c, "iZoomToggleKeyCode:Controls", zoomToggleKeyCode);
				zoomDefault = Read<float>(c, "fZoomDefault:Controls", zoomDefault);
				zoomZoomedIn = Read<float>(c, "fZoomZoomedIn:Controls", zoomZoomedIn);
				followPlayerCameraRotation = Read<bool>(c, "bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation);
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
			add("fOffsetX:Display", offsetX);
			add("fOffsetY:Display", offsetY);
			add("fScale:Display", scale);
			add("uShape:Display", shape);
			add("bShowOnGameStart:Display", showOnGameStart);
		}

		{
			using namespace controls;
			add("iHideKeyCode:Controls", static_cast<int>(hideKeyCode));
			add("iZoomToggleKeyCode:Controls", static_cast<int>(zoomToggleKeyCode));
			add("fZoomDefault:Controls", zoomDefault);
			add("fZoomZoomedIn:Controls", zoomZoomedIn);
			add("bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation);
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
		ok &= WriteFloat(kDisplaySection, "fOffsetX", display::offsetX);
		ok &= WriteFloat(kDisplaySection, "fOffsetY", display::offsetY);
		ok &= WriteFloat(kDisplaySection, "fScale", display::scale);
		ok &= WriteUInt(kDisplaySection, "uShape", display::shape);
		ok &= WriteBool(kDisplaySection, "bShowOnGameStart", display::showOnGameStart);

		ok &= WriteInt(kControlsSection, "iHideKeyCode", controls::hideKeyCode);
		ok &= WriteInt(kControlsSection, "iZoomToggleKeyCode", controls::zoomToggleKeyCode);
		ok &= WriteFloat(kControlsSection, "fZoomDefault", controls::zoomDefault);
		ok &= WriteFloat(kControlsSection, "fZoomZoomedIn", controls::zoomZoomedIn);
		ok &= WriteBool(kControlsSection, "bFollowPlayerCameraRotation", controls::followPlayerCameraRotation);

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
		display::offsetX = defaults.offsetX;
		display::offsetY = defaults.offsetY;

		display::scale = defaults.scale;
		display::shape = defaults.shape;
		display::showOnGameStart = defaults.showOnGameStart;

		controls::hideKeyCode = defaults.hideKeyCode;
		controls::zoomToggleKeyCode = defaults.zoomToggleKeyCode;
		controls::zoomDefault = defaults.zoomDefault;
		controls::zoomZoomedIn = defaults.zoomZoomedIn;
		controls::followPlayerCameraRotation = defaults.followPlayerCameraRotation;
	}

	const std::string& GetIniPath() { return iniPath; }
}
