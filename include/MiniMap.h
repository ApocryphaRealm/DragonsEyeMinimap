#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

namespace DEM
{
	class Minimap : public RE::HUDObject
	{
	public:
		static constexpr inline std::string_view path = "_level0.Minimap";

		// override HUDObject
		void Update() override{};								 // 01
		bool ProcessMessage(RE::UIMessage* a_message) override;	 // 02

		static void InitSingleton(const IUI::GFxDisplayObject& a_map)
		{
			if (!singleton)
			{
				static Minimap singletonInstance{ a_map };
				singleton = &singletonInstance;
			}
		}

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		void Advance();
		void PreRender();

		static Minimap* GetSingleton() { return singleton; }

		// members
		RE::HUDMenu* menu = nullptr;

	private:
		Minimap(const IUI::GFxDisplayObject& a_map) :
			RE::HUDObject{ a_map.GetMovieView() }
		{}

		void InitLocalMap();
		void InitScaleform();

		void Show(bool a_enable);

		void UpdateFogOfWar();
		void RenderOffscreen();

		std::array<RE::GFxValue, 2> GetCurrentLocationTitle() const;

		static inline Minimap* singleton = nullptr;

		// members
		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		const char* const& clearedStr = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared")->data.s;

		bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(501260, 0) }.get();
		bool& enableWaterRendering = *REL::Relocation<bool*>{ RELOCATION_ID(513342, 0) }.get();
	};
}