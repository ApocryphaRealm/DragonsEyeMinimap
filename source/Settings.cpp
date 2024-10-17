#include "Settings.h"

#include "utils/INISettingCollection.h"

namespace settings
{
	using namespace utils;

	void Init(const std::string& a_iniFileName)
	{
		INISettingCollection* iniSettingCollection = INISettingCollection::GetSingleton();

		{
			using namespace debug;
			iniSettingCollection->AddSettings(
				MakeSetting("uLogLevel:Debug", static_cast<std::uint32_t>(logLevel)));
		}

		{
			using namespace display;
			iniSettingCollection->AddSettings(
				MakeSetting("fPositionX:Display", positionX),
				MakeSetting("fPositionY:Display", positionY),
				MakeSetting("fScale:Display", scale),
				MakeSetting("uShape:Display", shape),
				MakeSetting("bColor:Display", color)
			);
		}

		{
			using namespace controls;
			iniSettingCollection->AddSettings(
				MakeSetting("bFollowPlayerCameraRotation:Controls", followPlayerCameraRotation),
				MakeSetting("fHoldDownToControlSecs:Controls", holdDownToControlSecs)
			);
		}

		if (!iniSettingCollection->ReadFromFile(a_iniFileName))
		{
			logger::warn("Could not read {}, falling back to default options", a_iniFileName);
		}

		{
			using namespace debug;
			logLevel = static_cast<logger::level>(iniSettingCollection->GetSetting<std::uint32_t>("uLogLevel:Debug"));
		}

		{
			using namespace display;
			positionX = iniSettingCollection->GetSetting<float>("fPositionX:Display");
			positionY = iniSettingCollection->GetSetting<float>("fPositionY:Display");
			scale = iniSettingCollection->GetSetting<float>("fScale:Display");
			shape = iniSettingCollection->GetSetting<std::uint32_t>("uShape:Display");
			color = iniSettingCollection->GetSetting<bool>("bColor:Display");
		}

		{
			using namespace controls;
			followPlayerCameraRotation = iniSettingCollection->GetSetting<bool>("bFollowPlayerCameraRotation:Controls");
			holdDownToControlSecs = iniSettingCollection->GetSetting<float>("fHoldDownToControlSecs:Controls");
		}
	}
}