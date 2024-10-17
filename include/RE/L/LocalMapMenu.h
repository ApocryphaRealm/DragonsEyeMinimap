#pragma once

#include "RE/B/BSCullingProcess.h"
#include "RE/B/BSTArray.h"
#include "RE/B/BSTSmartPointer.h"
#include "RE/G/GFxValue.h"
#include "RE/I/ImageSpaceShaderParam.h"
#include "RE/L/LocalMapCamera.h"
#include "RE/M/MenuEventHandler.h"
#include "RE/N/NiCamera.h"
#include "RE/N/NiSmartPointer.h"

namespace RE
{
	namespace BSGraphics
	{
		class BSShaderAccumulator;
	}

	class GridCellArray;
	class NiNode;

	class MapMarker
	{
	public:

		MapMarkerData* data;
		RefHandle ref;
		BSFixedString description;
		int unk18;
		int isDoor;
		int unk20;
		TESQuest* quest;
		bool unk30;
	};

	struct LocalMapMenu
	{
	public:
		struct FogOfWar
		{
			RE::NiNode* overlayHolder;		// 00
			std::uint16_t gridDivisions;	// 08
			RE::NiPoint3 minExtent;			// 0C
			RE::NiPoint3 maxExtent;			// 18
		};
		static_assert(sizeof(FogOfWar) == 0x28);

		struct LocalMapCullingProcess
		{
		public:
			struct UnkData
			{
				LocalMapCullingProcess* ptr;	// 00
				bool unk8;						// 08
			};
			static_assert(sizeof(UnkData) == 0x10);

			[[nodiscard]] inline LocalMapCamera* GetLocalMapCamera() const noexcept
			{
				return &REL::RelocateMember<LocalMapCamera>(this, 0x30260, 0x30270);
			}

			void SetLocalMapCamera(const LocalMapCamera& localMapCamera) noexcept
			{
				REL::RelocateMember<LocalMapCamera>(this, 0x30260, 0x30270) = localMapCamera;
			}

			void RenderOffScreen()
			{
				using func_t = decltype(&LocalMapMenu::LocalMapCullingProcess::RenderOffScreen);
				REL::Relocation<func_t> func{ RELOCATION_ID(16094, 16335) };

				func(this);
			}

			void CreateFogOfWar()
			{
				using func_t = decltype(&LocalMapMenu::LocalMapCullingProcess::CreateFogOfWar);
				REL::Relocation<func_t> func{ RELOCATION_ID(16095, 16336) };

				func(this);
			}

			static std::uint32_t CullTerrain(const RE::GridCellArray* a_gridCells, UnkData& a_unkData,
				const TESObjectCELL* a_cell)
			{
				using func_t = decltype(&LocalMapMenu::LocalMapCullingProcess::CullTerrain);
				REL::Relocation<func_t> func{ RELOCATION_ID(16098, 16339) };

				return func(a_gridCells, a_unkData, a_cell);
			}

			static std::uint32_t CullCellObjects(UnkData& a_unkData, const TESObjectCELL* a_cell)
			{
				using func_t = decltype(&LocalMapMenu::LocalMapCullingProcess::CullCellObjects);
				REL::Relocation<func_t> func{ RELOCATION_ID(16100, 16343) };

				return func(a_unkData, a_cell);
			}

			static int AttachFogOfWarOverlay(FogOfWar& a_fogOfWar, const TESObjectCELL* a_cell)
			{
				using func_t = decltype(&LocalMapMenu::LocalMapCullingProcess::AttachFogOfWarOverlay);
				REL::Relocation<func_t> func{ RELOCATION_ID(16101, 16344) };

				return func(a_fogOfWar, a_cell);
			}

			[[nodiscard]] inline NiPointer<BSGraphics::BSShaderAccumulator>& GetShaderAccumulator() const noexcept
			{
				return REL::RelocateMember<NiPointer<BSGraphics::BSShaderAccumulator>>(this, 0x302C8, 0x302D8);
			}

			[[nodiscard]] inline ImageSpaceShaderParam& GetImageSpaceShaderParam() const noexcept
			{
				return REL::RelocateMember<ImageSpaceShaderParam>(this, 0x302D0, 0x302E0);
			}

			[[nodiscard]] inline NiPointer<NiNode>& GetFogOfWarOverlay() const noexcept
			{
				return REL::RelocateMember<NiPointer<NiNode>>(this, 0x30358, 0x30368);
			}

			// members
			BSCullingProcess cullingProcess;					  // 00000
			CullJobDescriptor cullJobDesc;						  // 301F8
#ifndef ENABLE_SKYRIM_VR
			LocalMapCamera camera;								  // 30260
			NiPointer<BSGraphics::BSShaderAccumulator> unk302C8;  // 302C8
			ImageSpaceShaderParam unk302D0;						  // 302D0
			std::uint64_t unk30350;								  // 30350
			NiPointer<NiNode> fogOfWarOverlay;					  // 30358
#elif !defined(ENABLE_SKYRIM_AE) && !defined(ENABLE_SKYRIM_SE)
			std::uint64_t unk30260;								  // 30260
			std::uint64_t unk30268;								  // 30268
			LocalMapCamera camera;								  // 30270
			NiPointer<BSGraphics::BSShaderAccumulator> unk302D8;  // 302D8
			ImageSpaceShaderParam unk302E0;						  // 302E0
			std::uint64_t unk30360;								  // 30360
			NiPointer<NiNode> fogOfWarOverlay;					  // 30368
			BSTArray<void*> unk30370;							  // 30370
			BSTArray<void*> unk30388;							  // 30388
			BSTArray<void*> unk303A0;							  // 303A0
			void* unk303B8;										  // 303B8
			NiCamera* unk303C0;									  // 303C0
			std::uint32_t unk303C8;								  // 303C8
			std::uint32_t pad303CC;								  // 303CC
			std::uint64_t unk303D0;								  // 303D0
#else
			std::uint8_t unk30260[0x100];						  // 30260
#endif
		};
#ifndef ENABLE_SKYRIM_VR
		static_assert(sizeof(LocalMapCullingProcess) == 0x30360);
#elif !defined(ENABLE_SKYRIM_AE) && !defined(ENABLE_SKYRIM_SE)
		static_assert(sizeof(LocalMapCullingProcess) == 0x303D8);
#endif

		class InputHandler : public MenuEventHandler
		{
		public:
			inline static constexpr auto RTTI = RTTI_LocalMapMenu__InputHandler;

			~InputHandler() override;  // 00

			// override (MenuEventHandler)
			bool CanProcess(InputEvent* a_event) override;				// 01
			bool ProcessThumbstick(ThumbstickEvent* a_event) override;	// 03
			bool ProcessMouseMove(MouseMoveEvent* a_event) override;	// 04
			bool ProcessButton(ButtonEvent* a_event) override;			// 05

			// members
			LocalMapMenu* localMapMenu;	 // 10
		};
		static_assert(sizeof(InputHandler) == 0x18);

		struct RUNTIME_DATA
		{
			BSScaleformExternalTexture    imageData;			// 00
			GFxValue                      root;					// 18
			GFxValue                      iconDisplay;			// 30
			GFxMovieView*                 movieView;			// 48
			BSTSmartPointer<InputHandler> inputHandler;			// 50
			std::int32_t                  selectedMarkerIndex;	// 58
			bool                          enabled;				// 5C
			bool                          usingCursor;			// 5D
			bool                          inForeground;			// 5E
			std::uint8_t                  pad5F;				// 5F
		};
		static_assert(sizeof(RUNTIME_DATA) == 0x60);

		LocalMapMenu* Ctor()
		{
			using func_t = decltype(&LocalMapMenu::Ctor);
			REL::Relocation<func_t> func{ RELOCATION_ID(52076, 52964) };

			return func(this);
		}

		void Dtor()
		{
			using func_t = decltype(&LocalMapMenu::Dtor);
			REL::Relocation<func_t> func{ RELOCATION_ID(52077, 52965) };

			func(this);
		}

		void Advance()
		{
			using func_t = decltype(&LocalMapMenu::Advance);
			REL::Relocation<func_t> func{ RELOCATION_ID(52078, 52966) };

			func(this);
		}

		void Show(bool a_show)
		{
			using func_t = decltype(&LocalMapMenu::Show);
			REL::Relocation<func_t> func{ RELOCATION_ID(52079, 52969) };

			func(this, a_show);
		}

		void InitScaleform(RE::GFxMovie* const& a_movie)
		{
			using func_t = decltype(&LocalMapMenu::InitScaleform);
			REL::Relocation<func_t> func{ RELOCATION_ID(52080, 52970) };

			return func(this, a_movie);
		}

		void PopulateData()
		{
			using func_t = decltype(&LocalMapMenu::PopulateData);
			REL::Relocation<func_t> func{ RELOCATION_ID(52081, 52971) };

			func(this);
		}

		void RefreshMarkers()
		{
			using func_t = decltype(&LocalMapMenu::RefreshMarkers);
			REL::Relocation<func_t> func{ RELOCATION_ID(52090, 52980) };

			func(this);
		}

		[[nodiscard]] inline RUNTIME_DATA& GetRuntimeData() noexcept
		{
			return REL::RelocateMember<RUNTIME_DATA>(this, 0x303A0, 0x30418);
		}

		[[nodiscard]] inline const RUNTIME_DATA& GetRuntimeData() const noexcept
		{
			return REL::RelocateMember<RUNTIME_DATA>(this, 0x303A0, 0x30418);
		}

		// members
		BSTArray<MapMarker>    mapMarkers;			 // 00000
		GFxValue               markerData;			 // 00018
		GPointF                topLeft;				 // 00030
		GPointF                bottomRight;			 // 00038
		LocalMapCullingProcess localCullingProcess;	 // 00040
		RUNTIME_DATA           runtimeData;			 // 303A0, 30418
#if !defined(ENABLE_SKYRIM_AE) && !defined(ENABLE_SKYRIM_SE)
		std::uint32_t          unk30478;			 // 30478
		std::uint32_t          pad3047C;			 // 3047C
#endif
	};
#ifndef ENABLE_SKYRIM_VR
	static_assert(sizeof(LocalMapMenu) == 0x30400);
#elif !defined(ENABLE_SKYRIM_AE) && !defined(ENABLE_SKYRIM_SE)
	static_assert(sizeof(LocalMapMenu) == 0x30480);
#endif
}
