#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

namespace DEM
{
	struct ExtraMarker
	{
		struct Type
		{
			enum
			{
				kCombatant,
				kHostile,
				kGuard
			};
		};

		struct CreateData
		{
			enum
			{
				kName,
				kIconType,
				kStride
			};
		};

		struct RefreshData
		{
			enum
			{
				kX,
				kY,
				kStride
			};
		};
	};

	class Minimap : public RE::HUDObject
	{
	public:
		static constexpr inline std::string_view path = "_level0.HUDMovieBaseInstance.Minimap";

		// override HUDObject
		void Update() final {}											// 01
		bool ProcessMessage(RE::UIMessage* a_message) final;			// 02
		void RegisterHUDComponent(RE::FxDelegateArgs& a_params) final;	// 03

		static void InitSingleton(const IUI::GFxDisplayObject& a_gfxMinimap)
		{
			if (!singleton)
			{
				static Minimap singletonInstance{ a_gfxMinimap };
				singleton = &singletonInstance;
			}
		}

		static Minimap* GetSingleton() { return singleton; }

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		// Per frame steps: UpdateOnEnterFrame, Advance, PreRender
		void UpdateOnEnterFrame(const RE::FxDelegateArgs& a_delegateArgs);
		void Advance();
		void PreRender();

		// members
		IUI::GFxDisplayObject displayObj;

	private:
		Minimap(const IUI::GFxDisplayObject& a_gfxMinimap) :
			RE::HUDObject{ a_gfxMinimap.GetMovieView() },
			displayObj{ a_gfxMinimap }
		{}

		void InitLocalMap();

		void CreateMarkers();
		void RefreshMarkers();

		void UpdateFogOfWar();
		void RenderOffscreen();

		std::array<RE::GFxValue, 2> GetCurrentLocationTitle() const;

		static inline Minimap* singleton = nullptr;

		// members
		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::RUNTIME_DATA* localMap_ = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		RE::BSTArray<RE::NiPointer<RE::Actor>> enemyActors;
		RE::BSTArray<RE::NiPointer<RE::Actor>> hostileActors;
		RE::BSTArray<RE::NiPointer<RE::Actor>> guardActors;
		RE::GFxValue extraMarkerData;
		bool frameUpdatePending = true;

		const char* const& clearedStr = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared")->data.s;
		const float& localMapHeight = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalHeight:MapMenu")->data.f;
		const float& localMapMargin = *REL::Relocation<float*>{ RELOCATION_ID(234438, 189820) }.get();

		bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(501260, 359696) }.get();
		bool& byte_141E0DC5C = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 390865) }.get();
		bool& byte_141E0DC5D = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 390866) }.get();
		bool& enableWaterRendering = *REL::Relocation<bool*>{ RELOCATION_ID(513342, 391120) }.get();
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 414558) }.get();
		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 414746) }.get();
		bool& useMapBrightnessAndContrastBoost = *REL::Relocation<bool*>{ RELOCATION_ID(528107, 415052) }.get();
	};
}