#include <cctype>
#include <algorithm>
#include <fstream>
#include <map>
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

		// One offset pair per corner, so the INI names say which corner they belong to rather
		// than making the reader count indices.
		constexpr const char* kCornerNames[display::kAnchorCount] = { "TopLeft", "TopRight", "BottomLeft", "BottomRight" };

		std::string OffsetKey(char a_axis, int a_corner)
		{
			return std::format("fOffset{}{}", a_axis, kCornerNames[a_corner]);
		}

		

		std::string iniPath;
		std::string iniFileName;

		// The values the plugin compiles in, captured before the INI is read so that
		// "Restore defaults" means "what you would get with no INI at all".
		struct Defaults
		{
			logger::level logLevel;

			std::uint32_t anchor;
			std::array<float, display::kAnchorCount> offsetX;
			std::array<float, display::kAnchorCount> offsetY;
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

			logger::debug("Captured built-in defaults: anchor={}, scale={:.2f}, shape={}, hideKey={}, zoomToggleKey={}",
				defaults.anchor, defaults.scale, defaults.shape, defaults.hideKeyCode, defaults.zoomToggleKeyCode);
		}

		// One key a Save() is about to write. Queued rather than written on the spot so the
		// whole file is rewritten once at the end instead of once per key.
		struct PendingWrite
		{
			std::string section;
			std::string key;
			std::string value;
		};

		std::vector<PendingWrite> pendingWrites;

		bool EqualsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs)
		{
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		}

		std::string_view Trim(std::string_view a_text)
		{
			constexpr std::string_view kSpace = " \t\r\n";

			const std::size_t first = a_text.find_first_not_of(kSpace);

			if (first == std::string_view::npos)
			{
				return {};
			}

			return a_text.substr(first, a_text.find_last_not_of(kSpace) - first + 1);
		}

		// Queues a key for the next FlushPendingWrites(). Cannot fail on its own - the file is
		// only touched at flush time, so that is where a write error can surface.
		bool WriteRaw(const char* a_section, const char* a_key, const std::string& a_value)
		{
			pendingWrites.emplace_back(a_section, a_key, a_value);

			return true;
		}

		// Rewrites the INI with every queued change applied in place, leaving comments and any
		// keys this plugin does not know about untouched.
		//
		// Deliberately plain file I/O rather than WritePrivateProfileString. Mod Organizer 2's
		// usvfs does not reliably redirect the Win32 profile APIs: those calls returned success
		// and the plugin logged a successful save, while the file on disk was never written -
		// not in the mod folder, not in Overwrite - so every saved setting was silently lost on
		// the next load. Ordinary file reads and writes go through the VFS correctly.
		bool FlushPendingWrites()
		{
			if (pendingWrites.empty())
			{
				return true;
			}

			std::string text;

			{
				std::ifstream in(iniPath, std::ios::binary);

				if (in)
				{
					text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				}
				else
				{
					logger::warn("Could not read {} before saving; writing it from scratch", iniPath);
				}
			}

			// Keep whatever line ending the file already uses, so a save does not rewrite every
			// line of a CRLF file as LF (or the other way round) and bury the real change.
			const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";

			std::vector<std::string> lines;

			for (std::size_t start = 0; start <= text.size();)
			{
				const std::size_t end = text.find('\n', start);

				if (end == std::string::npos)
				{
					if (start < text.size())
					{
						lines.emplace_back(text.substr(start));
					}

					break;
				}

				std::string line = text.substr(start, end - start);

				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				lines.push_back(std::move(line));
				start = end + 1;
			}

			std::vector<bool> applied(pendingWrites.size(), false);

			// Pass one: replace any key that is already present under its own section.
			std::string currentSection;

			for (std::string& line : lines)
			{
				const std::string_view trimmed = Trim(line);

				if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
				{
					currentSection = std::string{ trimmed.substr(1, trimmed.size() - 2) };

					continue;
				}

				if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
				{
					continue;
				}

				const std::size_t separator = line.find('=');

				if (separator == std::string::npos)
				{
					continue;
				}

				const std::string_view key = Trim(std::string_view{ line }.substr(0, separator));

				for (std::size_t i = 0; i < pendingWrites.size(); ++i)
				{
					if (applied[i] || !EqualsIgnoreCase(currentSection, pendingWrites[i].section) ||
						!EqualsIgnoreCase(key, pendingWrites[i].key))
					{
						continue;
					}

					line = std::format("{}={}", key, pendingWrites[i].value);
					applied[i] = true;

					break;
				}
			}

			// Pass two: anything still unapplied is a key (or a whole section) the file does not
			// have yet, so append it at the end of its section, creating the section if needed.
			for (std::size_t i = 0; i < pendingWrites.size(); ++i)
			{
				if (applied[i])
				{
					continue;
				}

				const PendingWrite& pending = pendingWrites[i];

				std::size_t insertAt = lines.size();
				bool sectionFound = false;

				for (std::size_t l = 0; l < lines.size(); ++l)
				{
					const std::string_view trimmed = Trim(lines[l]);

					if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
					{
						continue;
					}

					if (sectionFound)
					{
						// The next section header - this key belongs just before it.
						insertAt = l;

						break;
					}

					if (EqualsIgnoreCase(trimmed.substr(1, trimmed.size() - 2), pending.section))
					{
						sectionFound = true;
						insertAt = lines.size();
					}
				}

				if (!sectionFound)
				{
					if (!lines.empty() && !Trim(lines.back()).empty())
					{
						lines.emplace_back();
					}

					lines.push_back(std::format("[{}]", pending.section));
					insertAt = lines.size();
				}

				// Step back over trailing blank lines so the key lands with its own section
				// rather than in the gap before the next one.
				while (insertAt > 0 && Trim(lines[insertAt - 1]).empty())
				{
					--insertAt;
				}

				lines.insert(lines.begin() + insertAt, std::format("{}={}", pending.key, pending.value));
				applied[i] = true;
			}

			std::string output;

			for (const std::string& line : lines)
			{
				output += line;
				output += newline;
			}

			std::ofstream out(iniPath, std::ios::binary | std::ios::trunc);

			if (!out)
			{
				logger::error("Could not open {} for writing; settings were not saved", iniPath);

				return false;
			}

			out.write(output.data(), static_cast<std::streamsize>(output.size()));
			out.close();

			if (!out)
			{
				logger::error("Could not write {}; settings were not saved", iniPath);

				return false;
			}

			logger::debug("FlushPendingWrites: wrote {} key(s) to {}", pendingWrites.size(), iniPath);

			return true;
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

		// ------------------------------------------------------------------------------------
		// THE SAVE/RELOAD ASYMMETRY, fixed 2026-08-27.
		//
		// Saving writes the INI with plain file I/O (see FlushPendingWrites and the comment
		// there about usvfs silently dropping WritePrivateProfileString). Reloading, however,
		// went through INISettingCollection::ReadFromFile - the GAME's collection, which uses
		// the Win32 profile APIs. PrivateProfileRedirector caches those, so a reload was served
		// the values from when the game started rather than the ones just written.
		//
		// Observed end to end on 2026-08-27: settings changed in game to anchor 0 / offset
		// (148, 0), saved, and the file on disk genuinely contained uAnchor=0 and
		// fOffsetXTopLeft=148 - while the reload immediately applied anchor 1 / offset (0, 0) /
		// scale 0.5, the compiled defaults. The write was never the problem; the read was.
		//
		// This is the same bug, with the same cause and the same fix, as the one already found
		// in Custom Difficulty UI. Read the file ourselves and prefer what it says.
		// ------------------------------------------------------------------------------------
		std::map<std::string, std::string> fileValues;

		std::string TrimCopy(std::string a_text)
		{
			const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
			a_text.erase(a_text.begin(), std::find_if(a_text.begin(), a_text.end(), notSpace));
			a_text.erase(std::find_if(a_text.rbegin(), a_text.rend(), notSpace).base(), a_text.end());
			return a_text;
		}

		// Parses the INI into the same "name:Section" keys the collection uses, so every existing
		// Read<> call site keeps working unchanged.
		void LoadFileValues(const std::string& a_path)
		{
			fileValues.clear();

			std::ifstream in(a_path, std::ios::binary);
			if (!in)
			{
				logger::warn("Could not open {} for a direct read; falling back to the settings collection", a_path);
				return;
			}

			std::string line;
			std::string section;

			while (std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				line = TrimCopy(line);

				if (line.empty() || line[0] == ';' || line[0] == '#')
				{
					continue;
				}

				if (line.front() == '[' && line.back() == ']')
				{
					section = TrimCopy(line.substr(1, line.size() - 2));
					continue;
				}

				const std::size_t eq = line.find('=');
				if (eq == std::string::npos)
				{
					continue;
				}

				const std::string key = TrimCopy(line.substr(0, eq));
				const std::string value = TrimCopy(line.substr(eq + 1));

				if (!key.empty())
				{
					fileValues[key + ":" + section] = value;
				}
			}

			logger::debug("Read {} value(s) directly from {}", fileValues.size(), a_path);
		}

		// Converts a raw INI string to T. Returns false when the text is not valid for the type,
		// so the caller keeps the collection's value rather than silently substituting a zero.
		template <typename T>
		bool ParseValue(const std::string& a_text, T& a_out)
		{
			if (a_text.empty())
			{
				return false;
			}

			try
			{
				if constexpr (std::is_same_v<T, bool>)
				{
					std::string lowered;
					lowered.reserve(a_text.size());
					for (unsigned char c : a_text) { lowered.push_back(static_cast<char>(std::tolower(c))); }

					if (lowered == "1" || lowered == "true"  || lowered == "yes") { a_out = true;  return true; }
					if (lowered == "0" || lowered == "false" || lowered == "no")  { a_out = false; return true; }
					return false;
				}
				else if constexpr (std::is_floating_point_v<T>)
				{
					a_out = static_cast<T>(std::stod(a_text));
					return true;
				}
				else if constexpr (std::is_signed_v<T>)
				{
					a_out = static_cast<T>(std::stoll(a_text));
					return true;
				}
				else
				{
					a_out = static_cast<T>(std::stoull(a_text));
					return true;
				}
			}
			catch (const std::exception&)
			{
				logger::warn("Value \"{}\" in the INI is not valid for its type; keeping the current value", a_text);
				return false;
			}
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
			// The file on disk is the truth. The collection may be serving a redirector's cache.
			if (const auto it = fileValues.find(a_name); it != fileValues.end())
			{
				T parsed{};
				if (ParseValue<T>(it->second, parsed))
				{
					return parsed;
				}
			}

			if (!a_collection->GetSetting(a_name))
			{
				logger::error("Setting \"{}\" is missing from the collection; keeping the current value", a_name);

				return a_fallback;
			}

			return a_collection->GetSetting<T>(a_name);
		}

		std::string ReadString(INISettingCollection* a_collection, const char* a_name, const std::string& a_fallback)
		{
			if (const auto it = fileValues.find(a_name); it != fileValues.end() && !it->second.empty())
			{
				return it->second;
			}

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

				logger::debug("Log level resolved to {}", static_cast<std::uint32_t>(logLevel));
			}

			{
				using namespace display;
				anchor = Read<std::uint32_t>(c, "uAnchor:Display", anchor);
				for (int corner = 0; corner < kAnchorCount; ++corner)
				{
					offsetX[corner] = Read<float>(c, (OffsetKey('X', corner) + ":Display").c_str(), offsetX[corner]);
					offsetY[corner] = Read<float>(c, (OffsetKey('Y', corner) + ":Display").c_str(), offsetY[corner]);
				}
				scale = Read<float>(c, "fScale:Display", scale);
				shape = Read<std::uint32_t>(c, "uShape:Display", shape);
				showOnGameStart = Read<bool>(c, "bShowOnGameStart:Display", showOnGameStart);

				const std::uint32_t safeAnchor = anchor < kAnchorCount ? anchor : 0;
				logger::debug("Display settings resolved: anchor={} ({}), scale={:.2f}, shape={}, showOnGameStart={}",
					anchor, kCornerNames[safeAnchor], scale, shape, showOnGameStart);
				logger::debug("Corner offsets resolved: TopLeft=({:.1f},{:.1f}) TopRight=({:.1f},{:.1f}) BottomLeft=({:.1f},{:.1f}) BottomRight=({:.1f},{:.1f})",
					offsetX[0], offsetY[0], offsetX[1], offsetY[1], offsetX[2], offsetY[2], offsetX[3], offsetY[3]);
			}

			{
				using namespace controls;
				hideKeyCode = Read<std::int32_t>(c, "iHideKeyCode:Controls", hideKeyCode);
				holdToPanSecs = Read<float>(c, "fHoldToPanSecs:Controls", holdToPanSecs);
				panHoldGamepadButton = Read<std::int32_t>(c, "iPanHoldGamepadButton:Controls", panHoldGamepadButton);
				gamepadHideButtonEnabled = Read<bool>(c, "bGamepadHideButton:Controls", gamepadHideButtonEnabled);
				zoomToggleKeyCode = Read<std::int32_t>(c, "iZoomToggleKeyCode:Controls", zoomToggleKeyCode);
				zoomDefault = Read<float>(c, "fZoomDefault:Controls", zoomDefault);
				zoomZoomedIn = Read<float>(c, "fZoomZoomedIn:Controls", zoomZoomedIn);
				followPlayerCameraRotation = Read<bool>(c, "bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation);
				compass::compassRing = Read<bool>(c, "bCompassRing:Compass", compass::compassRing);
				compass::questPointer = Read<bool>(c, "bQuestPointer:Compass", compass::questPointer);
				compass::metricUnits = Read<bool>(c, "bMetricUnits:Compass", compass::metricUnits);
				compass::ringGap = Read<float>(c, "fRingGap:Compass", compass::ringGap);
				compass::ringThickness = Read<float>(c, "fRingThickness:Compass", compass::ringThickness);
				compass::pointerSize = Read<float>(c, "fPointerSize:Compass", compass::pointerSize);
				compass::labelSize = Read<float>(c, "fLabelSize:Compass", compass::labelSize);
				compass::ringColor = Read<std::uint32_t>(c, "uRingColor:Compass", compass::ringColor);
				compass::northColor = Read<std::uint32_t>(c, "uNorthColor:Compass", compass::northColor);
				compass::pointerColor = Read<std::uint32_t>(c, "uPointerColor:Compass", compass::pointerColor);
				compass::discAlpha = Read<std::uint32_t>(c, "uDiscAlpha:Compass", compass::discAlpha);

				logger::debug("Key bindings resolved: hideKey={}, zoomToggleKey={}", hideKeyCode, zoomToggleKeyCode);
				logger::debug("Zoom presets resolved: default={:.2f}, zoomedIn={:.2f}, followPlayerCameraRotation={}",
					zoomDefault, zoomZoomedIn, followPlayerCameraRotation);
			}
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		CaptureDefaults();

		iniFileName = a_iniFileName;
		iniPath = std::filesystem::current_path().append("Data\\SKSE\\Plugins").append(a_iniFileName).string();

		logger::debug("Resolved INI path to {}", iniPath);

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
			for (int corner = 0; corner < kAnchorCount; ++corner)
			{
				add((OffsetKey('X', corner) + ":Display").c_str(), offsetX[corner]);
				add((OffsetKey('Y', corner) + ":Display").c_str(), offsetY[corner]);
			}
			add("fScale:Display", scale);
			add("uShape:Display", shape);
			add("bShowOnGameStart:Display", showOnGameStart);
		}

		{
			using namespace controls;
			add("iHideKeyCode:Controls", static_cast<int>(hideKeyCode));
			add("fHoldToPanSecs:Controls", holdToPanSecs);
			add("iPanHoldGamepadButton:Controls", static_cast<int>(panHoldGamepadButton));
			add("bGamepadHideButton:Controls", gamepadHideButtonEnabled);
			add("iZoomToggleKeyCode:Controls", static_cast<int>(zoomToggleKeyCode));
			add("fZoomDefault:Controls", zoomDefault);
			add("fZoomZoomedIn:Controls", zoomZoomedIn);
			add("bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation);
		}

		// DELIBERATELY NOT calling iniSettingCollection->ReadFromFile here.
		//
		// That call goes through GetPrivateProfileString, which PrivateProfileRedirector hooks -
		// and the moment it is asked about our INI, the Redirector loads the whole file into its
		// own cache. From then on it believes it owns that file. With the settings this modlist
		// ships (NativeWrite=0, SaveOnWrite=1, SaveOnGameSave=1, SaveOnProcessDetach=1) it will
		// write its cached copy back to disk on save and on exit - which would silently overwrite
		// the settings we wrote ourselves with plain file I/O, losing them between sessions.
		//
		// So we never introduce our INI to the Redirector at all. LoadFileValues reads the file
		// directly, and the collection is left holding only the compiled-in defaults registered
		// by AddChecked - which is exactly the fallback we want when a key is absent from the file.
		//
		// This also makes the plugin behave identically whether or not the Redirector is
		// installed: we no longer use the API it hooks, for reading or for writing.
		logger::debug("Settings are read directly from {}; the INI collection holds defaults only", a_iniFileName);

		LoadFileValues(iniPath);
		ReadFromCollection();

		logger::debug("Settings initialization complete");
	}

	bool Reload()
	{
		if (iniFileName.empty())
		{
			logger::error("Cannot reload settings before Init() has run");

			return false;
		}

		logger::debug("Reload requested for {}", iniPath);

		// Same reasoning as Init: re-reading through the collection would hand our INI to
		// PrivateProfileRedirector's cache, and a reload is precisely when we most need the file
		// on disk rather than a cache of it. LoadFileValues below reads it directly.

		LoadFileValues(iniPath);
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

		logger::debug("Saving settings to {}", iniPath);

		{
			const std::uint32_t safeAnchor = display::anchor < display::kAnchorCount ? display::anchor : 0;
			logger::debug("Display settings being saved: anchor={} ({}), scale={:.2f}, shape={}, showOnGameStart={}",
				display::anchor, kCornerNames[safeAnchor], display::scale, display::shape, display::showOnGameStart);
			logger::debug("Corner offsets being saved: TopLeft=({:.1f},{:.1f}) TopRight=({:.1f},{:.1f}) BottomLeft=({:.1f},{:.1f}) BottomRight=({:.1f},{:.1f})",
				display::offsetX[0], display::offsetY[0], display::offsetX[1], display::offsetY[1],
				display::offsetX[2], display::offsetY[2], display::offsetX[3], display::offsetY[3]);
			logger::debug("Key bindings being saved: hideKey={}, zoomToggleKey={}", controls::hideKeyCode, controls::zoomToggleKeyCode);
			logger::debug("Zoom presets being saved: default={:.2f}, zoomedIn={:.2f}, followPlayerCameraRotation={}",
				controls::zoomDefault, controls::zoomZoomedIn, controls::followPlayerCameraRotation);
		}

		bool ok = true;

		// Anything left over from an earlier Save() has already been flushed; starting clean
		// keeps a failed flush from writing a stale value on the next attempt.
		pendingWrites.clear();

		ok &= WriteUInt(kDebugSection, "uLogLevel", static_cast<std::uint32_t>(debug::logLevel));

		ok &= WriteUInt(kDisplaySection, "uAnchor", display::anchor);
		for (int corner = 0; corner < display::kAnchorCount; ++corner)
		{
			ok &= WriteFloat(kDisplaySection, OffsetKey('X', corner).c_str(), display::offsetX[corner]);
			ok &= WriteFloat(kDisplaySection, OffsetKey('Y', corner).c_str(), display::offsetY[corner]);
		}
		ok &= WriteFloat(kDisplaySection, "fScale", display::scale);
		ok &= WriteUInt(kDisplaySection, "uShape", display::shape);
		ok &= WriteBool(kDisplaySection, "bShowOnGameStart", display::showOnGameStart);

		ok &= WriteInt(kControlsSection, "iHideKeyCode", controls::hideKeyCode);
		ok &= WriteFloat(kControlsSection, "fHoldToPanSecs", controls::holdToPanSecs);
		ok &= WriteInt(kControlsSection, "iPanHoldGamepadButton", controls::panHoldGamepadButton);
		ok &= WriteInt(kControlsSection, "bGamepadHideButton", controls::gamepadHideButtonEnabled ? 1 : 0);
		ok &= WriteInt(kControlsSection, "iZoomToggleKeyCode", controls::zoomToggleKeyCode);
		ok &= WriteFloat(kControlsSection, "fZoomDefault", controls::zoomDefault);
		ok &= WriteFloat(kControlsSection, "fZoomZoomedIn", controls::zoomZoomedIn);
		ok &= WriteBool(kControlsSection, "bFollowPlayerCameraRotation", controls::followPlayerCameraRotation);

		// Write the file once, with every queued key applied. Until this succeeds nothing has
		// reached disk, so its result - not the queueing above - decides whether Save() worked.
		ok &= FlushPendingWrites();

		pendingWrites.clear();

		if (ok)
		{
			logger::info("Saved settings to {}", iniPath);
		}
		else
		{
			logger::error("Failed to save settings to {}", iniPath);
		}

		return ok;
	}

	bool SaveShowOnGameStart()
	{
		if (iniPath.empty())
		{
			logger::error("Cannot save bShowOnGameStart before Init() has run");

			return false;
		}

		pendingWrites.clear();

		bool ok = WriteBool(kDisplaySection, "bShowOnGameStart", display::showOnGameStart);

		ok &= FlushPendingWrites();

		pendingWrites.clear();

		if (ok)
		{
			logger::debug("Persisted bShowOnGameStart={} to {}", display::showOnGameStart, iniPath);
		}
		else
		{
			logger::error("Could not persist bShowOnGameStart to {}", iniPath);
		}

		return ok;
	}

	void RestoreDefaults()
	{
		// Logged from `defaults` before debug::logLevel itself is overwritten below, so the
		// confirmation is not silently dropped when the built-in default log level is below
		// Debug (which would otherwise suppress it the moment logLevel is restored).
		{
			const std::uint32_t safeAnchor = defaults.anchor < display::kAnchorCount ? defaults.anchor : 0;
			logger::debug("Restoring settings to built-in defaults: anchor={} ({}), scale={:.2f}, shape={}, showOnGameStart={}",
				defaults.anchor, kCornerNames[safeAnchor], defaults.scale, defaults.shape, defaults.showOnGameStart);
			logger::debug("Default corner offsets: TopLeft=({:.1f},{:.1f}) TopRight=({:.1f},{:.1f}) BottomLeft=({:.1f},{:.1f}) BottomRight=({:.1f},{:.1f})",
				defaults.offsetX[0], defaults.offsetY[0], defaults.offsetX[1], defaults.offsetY[1],
				defaults.offsetX[2], defaults.offsetY[2], defaults.offsetX[3], defaults.offsetY[3]);
			logger::debug("Default key bindings: hideKey={}, zoomToggleKey={}, zoomDefault={:.2f}, zoomZoomedIn={:.2f}, followPlayerCameraRotation={}",
				defaults.hideKeyCode, defaults.zoomToggleKeyCode, defaults.zoomDefault, defaults.zoomZoomedIn, defaults.followPlayerCameraRotation);
		}

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
