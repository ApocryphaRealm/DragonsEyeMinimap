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

		// Sets displayObj's own _visible. Show()/Hide() drive root, but IsVisible() - which gates
		// every per-frame update in Advance() - reads displayObj, so both have to be kept in step.
		void SetDisplayObjectVisible(bool a_visible);

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

		// One positioning pass. Returns false if it bailed out (nothing measurable to work with),
		// true otherwise, reporting through the out-params how far it actually moved the clip.
		// Not called directly - ApplyDisplaySettings() repeats it until the delta settles, because
		// the InitMap call at the end of each pass changes the artwork bounds the next pass reads.
		bool ApplyDisplaySettingsOnce(float& a_outDeltaX, float& a_outDeltaY);
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

		// Resolves localMap_->iconDisplay (and its MarkerData) if it is not resolved yet, and
		// reports whether it is usable now.
		//
		// InitLocalMap() used to look this up exactly once, immediately after root.Invoke("InitMap").
		// When that single attempt missed - which it does, because the Scaleform children are not
		// all built by then, the same late-settling behaviour pendingReapplyFrames exists to work
		// around - Advance() only ever re-tested the cached GFxValue, never re-queried. One early
		// miss therefore disabled every minimap marker for the whole session, no matter what any
		// visibility setting said. Retrying until it resolves is the fix.
		bool EnsureIconDisplay();

		void MeasureStage();

		// Converts a point in stage (screen) pixels into the minimap clip's parent space,
		// which is the space _x and _y are expressed in. Returns false if the parent or the
		// movie view cannot be reached.
		bool StageToParent(float a_stageX, float a_stageY, float& a_outX, float& a_outY);

		// The artwork's box in the parent's space, at whatever transform is applied right now.
		bool GetArtBoundsInParent(float& a_left, float& a_top, float& a_right, float& a_bottom);

		// Puts LocationName/ClearedHint below the map when it is anchored to the top of the
		// screen, and above it when anchored to the bottom - so the title is never the part
		// pushed against the screen edge. Safe to call repeatedly: it always computes an
		// absolute target from the authored layout, never a delta from wherever it currently
		// is, for the same reason the minimap's own position had to stop doing that.
		void ApplyTitlePosition();

		// Applies the current HUD Opacity slider to whichever of BackgroundArtSquare/
		// BackgroundArtCircle matches `shape`. That clip is the same DefineShape3 the frame
		// reskin recolored - background fill and frame outline baked into one shape, so there
		// is no way to fade only the fill through Scaleform without also fading the outline;
		// this fades the whole backing+frame together, the same way every other vanilla HUD
		// element responds to this slider. Safe to call every frame - it is one setting read
		// (a live reference, not a re-query) and one GFx call, and does nothing if localMap_
		// is not ready yet.
		void ApplyBackgroundOpacity();

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

		// The title's layout as authored, measured once by ApplyTitlePosition() the first time
		// it can reach LocalMapHolder/LocationName/ClearedHint. titleGap is the space between
		// the title and whichever map edge it started closest to; titleGroupHeight spans both
		// fields; the two offsets are each field's _y relative to the top of that combined
		// group, so their spacing to each other is preserved when the group moves.
		bool hasTitleGeometry = false;
		float titleGap = 0.0F;
		float titleGroupHeight = 0.0F;
		float titleNameOffset = 0.0F;
		float titleHintOffset = 0.0F;

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

		// ApplyDisplaySettings() runs twice during setup - once from the constructor, before
		// the map's children exist at all, and once from InitLocalMap(), in the same frame as
		// InitMap()/SetShape(). Neither has let Scaleform actually render a frame in between.
		// A single extra re-apply on the next real Advance() (added in 1.6.5) was not always
		// enough - the artwork's measured getBounds() can still be settling for a few frames
		// after that, which showed up as the map loading slightly off its flush position on
		// some loads rather than every one. This counts down kPendingReapplyFrames after
		// InitLocalMap(), re-applying once per frame until it reaches zero, so a late-settling
		// measurement gets caught instead of possibly needing a settings change to self-correct.
		// A safety cap, NOT a target. The old value of 6 was a guess at how many re-applies the
		// artwork would need to stop resizing, and it was sometimes one short: each re-apply moves
		// the map about 9.3 units, so running out of budget early left the minimap that far off
		// position with its frame past the screen edge. Confirmed by comparing two logs that
		// followed the same trajectory - 1.2.7 reached _x 482.29 in six, 1.2.8 needed a seventh
		// and stopped at 491.51. Settling is now decided by the position actually going still
		// (see kRequiredStableFrames); this only bounds how long that may take.
		static constexpr int kPendingReapplyFrames = 300;

		// How many consecutive re-applies must produce no movement before it counts as settled.
		// More than one, because the artwork can pause resizing for a frame and then continue.
		static constexpr int kRequiredStableFrames = 3;

		// Where the last ApplyDisplaySettings() left the clip, so Advance() can tell whether a
		// re-apply actually moved anything.
		float lastAppliedX = 0.0F;
		float lastAppliedY = 0.0F;
		int   displayStableFrames = 0;
		int pendingReapplyFrames = 0;

		// Vanilla's "Cleared" location-name suffix (sCleared). Kept as a nullable pointer
		// rather than an unchecked bound reference like the two settings below used to be -
		// "this pattern is used elsewhere in this file without incident" is exactly the
		// reasoning that let hudOpacitySetting's crash through, so it does not excuse these.
		// Checked wherever it is actually read (Advance()).
		RE::Setting* clearedStrSetting = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared");

		// Skyrim.ini's [MapMenu] local map height offset (fMapLocalHeight:MapMenu). Not
		// currently read anywhere in this file, but stored as a nullable pointer for the same
		// reason as clearedStrSetting above rather than left as an unchecked bound reference,
		// so a future read of it cannot reintroduce the pattern that crashed in 1.6.9.
		RE::Setting* localMapHeightSetting = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalHeight:MapMenu");

		// SkyrimPrefs.ini's [Display] HUD Opacity slider. Kept as a pointer rather than an
		// unchecked bound reference like the settings above - RE::INIPrefSettingCollection
		// (Prefs.ini) returned null for this setting on at least one real system, where
		// RE::INISettingCollection (Skyrim.ini) never has for fMapLocalHeight above. Taking
		// ->data.f off that null pointer didn't crash here (forming the reference is pure
		// pointer arithmetic), but reading through it later, in ApplyBackgroundOpacity(), did -
		// EXCEPTION_ACCESS_VIOLATION reading address 0x8, which is exactly nullptr plus that
		// field's offset. ApplyBackgroundOpacity() null-checks this before every read.
				//
		// The section is MAIN, not Display. SkyrimPrefs.ini keeps fHUDOpacity under [MAIN];
		// asking for it as "fHUDOpacity:Display" simply missed, so the null-safe fallback added
		// after the 1.1.8 crash engaged on every single launch and the background stayed fully
		// opaque no matter where the HUD Opacity slider was. Confirmed against a real
		// SkyrimPrefs.ini: fHUDOpacity=1.0000 sits in [MAIN].
		// Resolved lazily on first use rather than in a member initializer - the collections are
		// not necessarily populated when this object is constructed (CLAUDE.md rule 17). The
		// section name is not something to guess at: fHUDOpacity has now been looked for as
		// :Display (1.1.8-1.2.2) and :MAIN (1.2.3-1.2.5), and neither was found on a real system.
		// GetHUDOpacitySetting() tries the plausible names against both settings collections and,
		// if none match, logs every setting name containing "HUDOpacity" that the engine actually
		// has, so the next log answers the question instead of prompting another guess.
		static RE::Setting* GetHUDOpacitySetting();

		const float& localMapMargin = *REL::Relocation<float*>{ RELOCATION_ID(234438, 189820) }.get();
		const bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ REL::VariantID{ 501260, 359696, 0x1E70DFC } }.get();

		bool& useMapBrightnessAndContrastBoost = *REL::Relocation<bool*>{ RELOCATION_ID(528107, 415052) }.get();
		bool& nodeFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 390865) }.get();
		bool& nodeDrawFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 390866) }.get();
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 414558) }.get();
		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 414746) }.get();
	};
}