#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

#include "Settings.h"

#include "LMU/API.h"

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
		using Shape = LMU::PixelShaderProperty::Shape;
		using Style = LMU::PixelShaderProperty::Style;

		class InputHandler : public RE::MenuEventHandler
		{
		public:
			InputHandler(Minimap* a_miniMap)
			: miniMap{ a_miniMap }
			{}

			~InputHandler() final{};  // 00

			// override (RE::MenuEventHandler). ProcessThumbstick/ProcessMouseMove are not
			// overridden any more - the base class default (return false) is exactly what is
			// needed now that there is no camera panning to drive from them.
			bool CanProcess(RE::InputEvent* a_event) final;				 // 01
			bool ProcessButton(RE::ButtonEvent* a_event) final;			 // 05

			bool ProcessKeyboardOrMouseButton(RE::ButtonEvent* a_butonEvent);

		private:
			Minimap* miniMap;

			RE::MenuControls* menuControls = RE::MenuControls::GetSingleton();
		};

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

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		void Advance();
		void PreRender();

		// Controls
		bool IsVisible() const
		{
			RE::GFxValue::DisplayInfo displayInfo;
			displayObj.GetDisplayInfo(&displayInfo);

			return displayInfo.GetVisible();
		}

		bool IsShown() const
		{
			// localMap_ is the pointer actually dereferenced, so it is the one to guard.
			return localMap_ && localMap_->enabled;
		}

		void Show();
		void Hide();

		// The largest fScale that still keeps the minimap within a quarter of the screen, i.e.
		// within one screen quadrant. Returns the plain slider maximum until the clip has been
		// measured.
		float GetMaxScale() const;

		// True once InitLocalMap() has run and the Scaleform side is there to talk to.
		bool IsReady() const { return localMap_ != nullptr; }

		// Re-applies fPositionX / fPositionY / fScale to the Scaleform clip, and uShape to the
		// local map. Both must run on the main thread; the settings menu queues them there.
		void ApplyDisplaySettings();
		void ApplyShapeSetting();

		// The map's current zoom, as the camera holds it.
		float GetMapZoom() const
		{
			// The menu polls this from the render thread while InitLocalMap may still be
			// running, so check both links of the chain rather than only the first.
			return cameraContext && cameraContext->defaultState ? cameraContext->defaultState->zoom : 0.0F;
		}

		// Steer the zoom to an absolute value. Main thread only.
		void SetMapZoom(float a_zoom);

		// Jump to whichever of the two zoom presets is further from where we are. Main thread only.
		// Alternates the map zoom between settings::controls::zoomDefault and zoomZoomedIn.
		void ToggleZoomPreset();

		// Local Map Upgrade interface
		static inline void (*SetPixelShaderProperties)(LMU::PixelShaderProperty::Shape a_shape, LMU::PixelShaderProperty::Style a_style);
		static inline void (*GetPixelShaderProperties)(LMU::PixelShaderProperty::Shape& a_shape, LMU::PixelShaderProperty::Style& a_style);

	private:
		Minimap(const IUI::GFxDisplayObject& a_gfxMinimap) :
			RE::HUDObject{ a_gfxMinimap.GetMovieView() },
			displayObj{ a_gfxMinimap }
		{
			if (displayObj.HasMember("Minimap"))
			{
				// Record the clip exactly as authored, before anything has moved or scaled it.
				baseX = static_cast<float>(displayObj.GetMember("_x").GetNumber());
				baseY = static_cast<float>(displayObj.GetMember("_y").GetNumber());
				baseXScale = static_cast<float>(displayObj.GetMember("_xscale").GetNumber());
				baseYScale = static_cast<float>(displayObj.GetMember("_yscale").GetNumber());

				MeasureStage();

				// One code path for the initial layout and every later change, so the two
				// cannot drift apart.
				ApplyDisplaySettings();
			}
		}

		void InitLocalMap();

		void MeasureStage();

		// Converts a point in stage (screen) pixels into the minimap clip's parent space,
		// which is the space _x and _y are expressed in. Returns false if the parent or the
		// movie view cannot be reached.
		bool StageToParent(float a_stageX, float a_stageY, float& a_outX, float& a_outY);

		// The artwork's box in the parent's space, at whatever transform is applied right now.
		bool GetArtBoundsInParent(float& a_left, float& a_top, float& a_right, float& a_bottom);

		void UpdateFogOfWar();
		void RenderOffScreen();
		void ClearTerrainRenderPasses(RE::NiPointer<RE::NiAVObject>& a_object);
		void CullTerrain(const RE::GridCellArray* a_gridCells, RE::LocalMapMenu::LocalMapCullingProcess::UnkData& a_unkData,
						 const RE::TESObjectCELL* a_cell);

		static inline Minimap* singleton = nullptr;

		// members
		IUI::GFxDisplayObject displayObj;

		// The clip's transform as authored, before any positioning or scaling was applied.
		float baseX = 0.0F;
		float baseY = 0.0F;
		float baseXScale = 100.0F;
		float baseYScale = 100.0F;

		// The artwork's size at scale 1, learned the first time it is measured for real. Used
		// only to work out the largest scale that still fits a quarter of the screen.
		float artWidthAtScaleOne = 0.0F;
		float artHeightAtScaleOne = 0.0F;

		// Screen size in stage pixels.
		float stageWidth = 0.0F;
		float stageHeight = 0.0F;

		Shape shape = static_cast<Shape>(settings::display::shape);

		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::RUNTIME_DATA* localMap_ = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		float minCamFrustumHalfWidth = 0.0F;
		float minCamFrustumHalfHeight = 0.0F;

		// Which of the two zoom presets ToggleZoomPreset last chose. Not persisted - it
		// resets to "default" every game launch, which is a reasonable place to start from.
		bool zoomedIn = false;

		RE::BSTSmartPointer<InputHandler> inputHandler = RE::make_smart<InputHandler>(this);

		bool isCameraUpdatePending = true;

		const char* const& clearedStr = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared")->data.s;
		const float& localMapHeight = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalHeight:MapMenu")->data.f;

		const float& localMapMargin = *REL::Relocation<float*>{ RELOCATION_ID(234438, 189820) }.get();
		const bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ REL::VariantID{ 501260, 359696, 0x1E70DFC } }.get();

		bool& useMapBrightnessAndContrastBoost = *REL::Relocation<bool*>{ RELOCATION_ID(528107, 415052) }.get();
		bool& nodeFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 390865) }.get();
		bool& nodeDrawFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 390866) }.get();
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 414558) }.get();
		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 414746) }.get();
	};
}