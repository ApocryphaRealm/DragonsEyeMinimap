#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

#include "Settings.h"

namespace DEM
{
	struct ExtraMarker
	{
		struct Type
		{
			enum
			{
				kEnemy,
				kHostile,
				kGuard,
				kDead,
				kTotal
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
		enum class Shape
		{
			kSquare,
			kCircle
		};

		class InputHandler : public RE::MenuEventHandler
		{
		public:
			InputHandler(Minimap* a_miniMap) 
			: miniMap{ a_miniMap }
			{}

			~InputHandler() final{};  // 00

			// override (RE::MenuEventHandler)
			bool CanProcess(RE::InputEvent* a_event) final;				 // 01
			bool ProcessThumbstick(RE::ThumbstickEvent* a_event) final;	 // 03
			bool ProcessMouseMove(RE::MouseMoveEvent* a_event) final;	 // 04
			bool ProcessButton(RE::ButtonEvent* a_event) final;			 // 05

			Minimap* miniMap;
		};

		friend class InputHandler;

		static constexpr inline std::string_view path = "_level0.HUDMovieBaseInstance.Minimap";

		// override (RE::HUDObject)
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

		bool IsVisible() const
		{
			RE::GFxValue::DisplayInfo displayInfo;
			displayObj.GetDisplayInfo(&displayInfo);

			return displayInfo.GetVisible();
		}

		bool IsShown() const
		{
			return localMap && localMap_->enabled;
		}

		void Show()
		{
			localMap_->inForeground = localMap_->enabled = true;
			localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ true });
		}

		void Hide()
		{
			localMap_->inForeground = localMap_->enabled = false;
			localMap_->root.Invoke("Show", std::array<RE::GFxValue, 1>{ false });
		}

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		void Advance();
		void PreRender();

		// members
		IUI::GFxDisplayObject displayObj;

	private:
		Minimap(const IUI::GFxDisplayObject& a_gfxMinimap) :
			RE::HUDObject{ a_gfxMinimap.GetMovieView() },
			displayObj{ a_gfxMinimap }
		{
			if (displayObj.HasMember("Minimap"))
			{
				displayObj.Invoke("Minimap", settings::display::positionX, settings::display::positionY);

				float width = displayObj.GetMember("_width").GetNumber();
				displayObj.SetMember("_width", width * settings::display::scale);

				float height = displayObj.GetMember("_height").GetNumber();
				displayObj.SetMember("_height", height * settings::display::scale);
			}
		}

		void InitLocalMap();

		void UpdateFogOfWar();
		void RenderOffscreen();
		void ClearTerrainRenderPasses(RE::NiPointer<RE::NiAVObject>& a_object);
		void CullTerrain(const RE::GridCellArray* a_gridCells, RE::LocalMapMenu::LocalMapCullingProcess::UnkData& a_unkData,
						 const RE::TESObjectCELL* a_cell);

		// Controls
		void FoldControls();
		void UnfoldControls();

		static inline Minimap* singleton = nullptr;

		// members
		Shape shape = static_cast<Shape>(settings::display::shape);

		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::RUNTIME_DATA* localMap_ = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		float minCamFrustumHalfWidth = 0.0F;
		float minCamFrustumHalfHeight = 0.0F;

		RE::BSTSmartPointer<InputHandler> inputHandler = RE::make_smart<InputHandler>(this);
		bool inputControlledMode = false;

		bool isCameraUpdatePending = true;

		const char* const& clearedStr = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared")->data.s;
		const float& localMapHeight = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalHeight:MapMenu")->data.f;
		const float& localMapPanSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalMousePanSpeed:MapMenu")->data.f;
		const float& localMapMouseZoomSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalMouseZoomSpeed:MapMenu")->data.f;
		const float& localMapGamepadZoomSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalGamepadZoomSpeed:MapMenu")->data.f;

		const float& localMapMargin = *REL::Relocation<float*>{ RELOCATION_ID(234438, 189820) }.get();
		const bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(501260, 359696) }.get();
	};
}