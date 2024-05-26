#pragma once

#include "RE/B/BSRenderPass.h"
#include "RE/B/BSTArray.h"
#include "RE/B/BSTHashMap.h"
#include "RE/B/BSTList.h"

namespace RE
{
	class BSBatchRenderer
	{
	public:
		struct PersistentPassList
		{
			BSRenderPass* head;	// 0
			BSRenderPass* tail;	// 8
		};
		static_assert(sizeof(PersistentPassList) == 0x10);

		struct GeometryGroup
		{
			BSBatchRenderer* batchRenderer;	 // 00
			PersistentPassList passList;	 // 08
			void* unk08;					 // 18
			float depth;					 // 20 - Distance from geometry to camera location
			std::uint16_t count;			 // 24
			std::uint8_t flags;				 // 26
		};
		static_assert(sizeof(GeometryGroup) == 0x28);

		struct PassGroup
		{
			BSRenderPass* passes[5];
			std::uint32_t validPassBits;	// OR'd with (1 << PassIndex)
		};

		inline static constexpr auto RTTI = RTTI_BSBatchRenderer;

		virtual ~BSBatchRenderer();	 // 00

		// add
		virtual void Unk_01(BSRenderPass* a_renderPass, std::uint32_t a_technique);											 // 01
		virtual void Unk_02(BSRenderPass* a_renderPass, std::uint32_t a_technique);											 // 02
		virtual void Unk_03(std::uint32_t a_startTechniqueID, std::uint32_t a_endTechniqueID, std::uint32_t a_renderFlags);	 // 03

		// members
		BSTArray<PassGroup> renderPass;							 // 008
		BSTHashMap<std::uint32_t, std::uint32_t> renderPassMap;	 // 020 - Technique ID -> Index in renderPass
		std::uint32_t currentFirstPass;							 // 050
		std::uint32_t currentLastPass;							 // 054
		BSSimpleList<std::uint32_t> activePassIndexList;		 // 058
		std::int32_t groupingAlphas;							 // 068
		bool autoClearPasses;									 // 06C
		GeometryGroup* geometryGroups[16];						 // 070
		GeometryGroup* alphaGroup;								 // 0F0
		void* unk0F8;											 // 0F8
		void* unk100;											 // 100
	private:
		KEEP_FOR_RE()
	};
	static_assert(sizeof(BSBatchRenderer) == 0x108);
}
