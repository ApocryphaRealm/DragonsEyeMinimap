#pragma once

#if RUNTIME_LINE == 17
// Skyrim 1.7 line: CommonLibSSE-NG 7.2 ships this type with its 1.7 layout, so that one is used (see CMakeLists.txt, line 17).
#	include <include/RE/H/HUDData.h>
#else

#include "RE/B/BSString.h"
#include "RE/E/ExtraMapMarker.h"
#include "RE/I/IUIMessageData.h"

namespace RE
{
	class HUDData : public IUIMessageData
	{
	public:
		inline static constexpr auto RTTI = RTTI_HUDData;

		enum class Type
		{
			kUnk0 = 0,
			kNotification = 1,
			kSetCroshairTarget = 2,
			kSetCroshairTargetTextOnly = 3,
			kSetLoadDoorInfo = 4,
			kShowSubtitle = 5,
			kHideSubtitle = 6,
			kShowArrowCount = 7,
			kHideArrowCount = 8,
			kSetSubtitlesEnabled = 9,
			kSetBlinking = 11,
			kSetFadeOut = 12,
			kSetPct = 13,
			kQuestStarted = 14,
			kQuestComplete = 15,
			kQuestFailed = 16,
			kObjectiveStarted = 17,
			kObjectiveComplete = 18,
			kObjectiveFailed = 19,
			kSkillIncrease = 20,
			kWordOfPowerLearned = 21,
			kDragonSoulAbsorbed = 22,
			kShowElements = 23,
			kCrosshairSneak = 24,
			kLocationDiscovery = 26,
			kFavor = 27,
			kValidateCrosshair = 28,
			kSetLocationName = 29,
			kShowTutorialHintText = 30,
			kSetCrosshairEnabled = 31,
			kSetDisplayInfo = 32,
			kRefreshActivateButtonArt = 33,
			kSurvival = 34,
		};

		~HUDData() override;  // 00

		// members
		stl::enumeration<Type, std::uint32_t> type;					 // 10
		std::uint32_t pad14;										 // 14
		BSString text;												 // 18
		ObjectRefHandle crossHairRef;								 // 28
		std::uint32_t pad2C;										 // 2C
		TESQuest* quest;											 // 30
		TESWordOfPower* wordOfPower;								 // 38
		std::uint8_t showHide;										 // 40
		std::uint8_t pad41;											 // 41
		std::uint16_t pad42;										 // 42
		stl::enumeration<RE::MARKER_TYPE, std::uint32_t> discovery;	 // 44
	};
	static_assert(sizeof(HUDData) == 0x48);
}
#endif  // RUNTIME_LINE
