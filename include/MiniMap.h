#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

namespace DEM
{
	class Minimap : public RE::HUDObject
	{
	public:

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

		static Minimap* GetSingleton() { return singleton; }

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		void Advance(RE::HUDMenu* a_hudMenu);
		void PreRender(RE::HUDMenu* a_hudMenu);

		void UpdateFogOfWar();
		void RenderOffscreen();

	private:
		Minimap(const IUI::GFxDisplayObject& a_map) :
			RE::HUDObject{ a_map.GetMovieView() }
		{}

		void InitLocalMap();

		static inline Minimap* singleton = nullptr;

		// members
		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(501260, 0) }.get();
	};
}