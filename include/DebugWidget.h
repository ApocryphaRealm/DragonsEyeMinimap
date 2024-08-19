#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

namespace DEM
{
	class DebugWidget : public RE::HUDObject
	{
	public:
		static constexpr inline std::string_view path = "_level0.DebugWidget";

		// override (RE::HUDObject)
		void Update() final												// 01
		{
			displayObj.Invoke("ShowRenderPasses", renderPasses);
		}

		void RegisterHUDComponent(RE::FxDelegateArgs& a_params) final	// 03
		{
			RE::HUDObject::RegisterHUDComponent(a_params);
			displayObj.Invoke("AddToHudElements");
		}

		static void InitSingleton(const IUI::GFxDisplayObject& a_gfxMinimap)
		{
			if (!singleton)
			{
				static DebugWidget singletonInstance{ a_gfxMinimap };
				singleton = &singletonInstance;
			}
		}

		static DebugWidget* GetSingleton() { return singleton; }

		void SetRenderPasses(std::uint32_t a_renderPasses) { renderPasses = a_renderPasses; }

	private:
		DebugWidget(const IUI::GFxDisplayObject& a_gfxObj) :
			RE::HUDObject{ a_gfxObj.GetMovieView() },
			displayObj{ a_gfxObj }
		{
			if (displayObj.HasMember("DebugWidget"))
			{
				displayObj.Invoke("DebugWidget", 0, 0);
			}
		}

		static inline DebugWidget* singleton = nullptr;

		// members
		IUI::GFxDisplayObject displayObj;

		std::uint32_t renderPasses = 0;
	};
}