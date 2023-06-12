#include "Hooks.h"

#include "utils/Logger.h"

#include "RE/H/HUDMenu.h"

#include "RE/RTTI.h"

#include "Minimap.h"


void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate)
{
	hooks::HUDMenu::Accept(a_hudMenu, a_gameDelegate);

	a_gameDelegate->Process("SetLocalMapExtents",
		[](const RE::FxDelegateArgs& a_delegateArgs) -> void
		{
			DEM::Minimap::GetSingleton()->SetLocalMapExtents(a_delegateArgs);
		});
}

void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime)
{
	hooks::HUDMenu::AdvanceMovie(a_hudMenu, a_interval, a_currentTime);

	DEM::Minimap* minimap = DEM::Minimap::GetSingleton();
	if (minimap)
	{
		minimap->Advance(a_hudMenu);
	}
}

void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu)
{
	if (!RE::UI::GetSingleton()->IsMenuOpen(RE::MapMenu::MENU_NAME))
	{
		DEM::Minimap* minimap = DEM::Minimap::GetSingleton();
		if (minimap)
		{
			minimap->PreRender(a_hudMenu);
		}
	}

	hooks::HUDMenu::PreDisplay(a_hudMenu);
}

void LocalMapMenuTest(RE::LocalMapMenu* a_localMapMenu, const char* a_fileName)
{
	RE::BSFixedString fileName = a_fileName;

	a_localMapMenu->GetRuntimeData().imageData.BindToVirtualTarget(fileName, RE::RENDER_TARGET::kLOCAL_MAP);

	RE::NiPoint3 maxExtentExtra{ 0, 0, 40000 };

	auto localMapCamera = a_localMapMenu->localCullingProcess.GetLocalMapCamera();
	localMapCamera->defaultState->minFrustumHalfWidth = 4000.0F * 0.5F;
	localMapCamera->defaultState->minFrustumHalfHeight = localMapCamera->defaultState->minFrustumHalfWidth * 9.0F / 16.0F;
	a_localMapMenu->localCullingProcess.Setup();
}

// This function is here only for testing
bool WorldPtToScreenPt3(RE::NiCamera* a_camera, const RE::NiPoint3& a_location, float& a_xOut, float& a_yOut, float& a_zOut, float a_zeroTolerance)
{
	auto playerLocation = RE::PlayerCharacter::GetSingleton()->GetPosition();

	if (playerLocation == a_location)
	{
		auto& runtimeData = a_camera->GetRuntimeData();
		auto& runtimeData2 = a_camera->GetRuntimeData2();
		
		if (auto mapMenu = static_cast<RE::MapMenu*>(RE::UI::GetSingleton()->GetMenu(RE::MapMenu::MENU_NAME).get()))
		{
			RE::LocalMapCamera* localMapCamera = mapMenu->GetRuntimeData().localMapMenu.localCullingProcess.GetLocalMapCamera();
			//RE::LocalMapCamera* miniMapCamera = DEM::Minimap::GetSingleton()->GetLocalMap()->localCullingProcess.GetLocalMapCamera();

			int stop = 1;
		}
	}
	return hooks::NiCamera::WorldPtToScreenPt3(a_camera, a_location, a_xOut, a_yOut, a_zOut, a_zeroTolerance);
}