#pragma once

#include "RE/N/NiObject.h"
#include "RE/N/NiCamera.h"

namespace RE
{
	class NiAccumulator : public NiObject
	{
	public:
		~NiAccumulator() override; // 00

		// add
		virtual void StartAccumulating(NiCamera* a_camera);	 // 25
		virtual void FinishAccumulating();					 // 26
		virtual void RegisterObjectArray();					 // 27
		virtual void Unk_28(void);							 // 28
		virtual void Unk_29(void);							 // 29

		// members
		const NiCamera* camera; // 10
	};
	static_assert(sizeof(NiAccumulator) == 0x18);

	class NiBackToFrontAccumulator : public NiAccumulator
	{
	public:
		~NiBackToFrontAccumulator() override;  // 00

		// members
		std::uint64_t unk18;	// 18
		std::uint64_t unk20;	// 20
		std::uint64_t unk28;	// 28
		std::int32_t numItems;	// 30
		std::int32_t maxItems;	// 34
		BSGeometry** items;		// 38
		float* depths;			// 40
		std::int32_t currItem;	// 48
		std::uint32_t pad4C;	// 4C
	};
	static_assert(sizeof(NiBackToFrontAccumulator) == 0x50);

	class NiAlphaAccumulator : public NiBackToFrontAccumulator
	{
	public:
		~NiAlphaAccumulator() override;	 // 00

		// members
		bool observeNoSortHint;	  // 50
		bool sortByClosestPoint;  // 51
		bool interfaceSort;		  // 52
		std::uint8_t pad53;		  // 53
		std::uint32_t pad54;	  // 54
	};
	static_assert(sizeof(NiAlphaAccumulator) == 0x58);

	class BSShaderAccumulator : public NiAlphaAccumulator
	{
	public:
		~BSShaderAccumulator() override;  // 00

		// add
		virtual void FinishAccumulatingDispatch(std::uint32_t a_renderFlags);  // 2A
		virtual void Unk_2B(uint32_t a_unk0);								   // 2B
		virtual void Unk_2C(void);											   // 2C

		void* sub_1412CCF90(bool a_unk0)
		{
			using func_t = decltype(&BSShaderAccumulator::sub_1412CCF90);
			REL::Relocation<func_t> func{ RELOCATION_ID(99964, 0) };

			return func(this, a_unk0);
		}

		// members
		std::uint64_t unk058[(0x128 - 0x58) >> 3];	// 058
		bool firstPerson;							// 128
		std::uint8_t unk129;						// 129
		std::uint8_t unk12A;						// 12A
		std::uint8_t unk12B;						// 12B
		bool drawDecals;							// 12C
		std::uint8_t pad12D;						// 12D
		std::uint16_t pad12E;						// 12E
		BSBatchRenderer* batchRenderer;				// 130
		std::uint32_t currentPass;					// 138
		std::uint32_t currentBucket;				// 13C
		bool currentActive;							// 140
		std::uint8_t pad141;						// 141
		std::uint16_t pad142;						// 142
		std::uint32_t pad144;						// 144
		ShadowSceneNode* activeShadowSceneNode;		// 148
		std::uint32_t renderMode;					// 150
		std::uint32_t unk154;						// 154
		std::uint64_t unk158;						// 158
		std::uint64_t unk160;						// 160
		std::uint32_t unk168;						// 168
		NiPoint3 eyePosition;						// 16C
		std::uint64_t unk178;						// 178
	};
	static_assert(sizeof(BSShaderAccumulator) == 0x180);
}