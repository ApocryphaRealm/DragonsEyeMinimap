#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

#include "Settings.h"

#include <chrono>
#include <mutex>

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
			{
				// RE::MenuEventHandler's `registered` is an ENGINE-managed field with no
				// initializer: MenuControls::AddHandler/RemoveHandler maintain it, but a
				// freshly constructed handler holds uninitialized memory there. When the
				// garbage read as true, ProcessMessage's `!registered` guard skipped
				// AddHandler forever and every key/controller input was dead - the author's
				// 2026-08-31 report ("hide and zoom are not working anymore"), a heisenbug
				// that flipped with the 1.5.8 binary layout. Start it FALSE explicitly.
				registered = false;
			}

			~InputHandler() final{};  // 00

			// override (RE::MenuEventHandler). ProcessMouseMove is back (1.5.9, the author): holding the
			// hide key pans the map with the mouse, the original control scheme, on the same key.
			bool CanProcess(RE::InputEvent* a_event) final;				 // 01
			bool ProcessThumbstick(RE::ThumbstickEvent* a_event) final; // 03 - RIGHT stick pans while holding (design decision, 2026-08-30)
			bool ProcessMouseMove(RE::MouseMoveEvent* a_event) final;	 // 04
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

		// The artwork rect as last positioned, in STAGE pixels, published for the pointer add-on
		// (DEM_GetMinimapStageRect, 1.5.9). Written on the UI thread at the end of
		// ApplyDisplaySettingsOnce, read from the framework's render thread - hence the mutex.
		struct StageRect { float left = 0, top = 0, right = 0, bottom = 0; bool valid = false; };
		static inline std::mutex stageRectLock;
		static inline StageRect stageRect;
		static inline int stageRectCorner = 0;
		static inline bool stageRectFromShown = false;
		static inline float stageRectStageW = 0.0F, stageRectStageH = 0.0F;


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

		// Reports the HUD's current mode stack, for diagnosing who is hiding this element.
		//
		// HUDMovieBaseInstance keeps a stack of mode names and hides any element that has no
		// property matching the mode on top. Something is closing this element's visibility every
		// 100-200ms during play, and knowing WHICH mode is on the stack at that moment names the
		// flag the clip is missing, instead of guessing at the seventeen possibilities.
		std::string DescribeHudModes() const;

		// Logs which HUD mode flags the clip actually owns, which is what ShowElements tests.
		void ReportModeFlagOwnership() const;


		// Sets displayObj's own _visible. Show()/Hide() drive root, but IsVisible() - which gates
		// every per-frame update in Advance() - reads displayObj, so both have to be kept in step.
		void SetDisplayObjectVisible(bool a_visible);

		// a_persist writes bShowOnGameStart to the INI. TRUE for a deliberate settings-page
		// choice; FALSE for a runtime show/hide, which is transient state and should not decide
		// what happens on the next game start. Every toggle used to persist, so quitting on an
		// odd-numbered keypress left the minimap hidden on disk - and the player never chose
		// that, they just pressed the key an odd number of times (CLAUDE.md rule 16 case,
		// 2026-08-26).
		void Show(bool a_persist = true);
		void Hide(bool a_persist = true);

		// The largest fScale that still keeps the minimap within a quarter of the screen, i.e.
		// within one screen quadrant. Returns the plain slider maximum until the clip has been
		// measured.
		float GetMaxScale() const;

		// True once InitLocalMap() has run and the Scaleform side is there to talk to.
		bool IsReady() const { return localMap_ != nullptr; }

		// The HUD movie that hosts the minimap - CompassRing draws its own clip on its root.
		RE::GFxMovieView* GetHudMovieView() { return displayObj.GetMovieView(); }

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

		// Points the local-map camera at the player and applies the rotation Advance() just
		// chose. Called from Advance BEFORE the markers are placed, because the engine places
		// them from this camera: until 1.6.3 it was only called later in the frame, from
		// PreRender, so every marker was positioned with the PREVIOUS frame's camera while the
		// picture underneath it was drawn with the current one.
		void UpdateCamera();

		void UpdateFogOfWar();

		// Whether the world image inside the frame may be redrawn on this frame - see the
		// [Rendering] settings. Returns false while the world is still streaming in after a
		// cell change, and while the configured redraw interval has not elapsed. It keeps its
		// own state, so it is called exactly once per frame, from PreRender.
		bool ShouldRedrawWorld();

		// True when every cell this frame would cull is attached with its 3D built and the shadow
		// scene children the off-screen render walks are real objects. a_reason names the first
		// thing that is not, for the log.
		bool WorldIsSteady(const char*& a_reason);

		void RenderOffScreen();
		void ClearTerrainRenderPasses(RE::NiPointer<RE::NiAVObject>& a_object);
		void CullTerrain(const RE::GridCellArray* a_gridCells, RE::LocalMapMenu::LocalMapCullingProcess::UnkData& a_unkData,
						 const RE::TESObjectCELL* a_cell);

		static inline Minimap* singleton = nullptr;

		// Hold-to-pan (1.5.9): true while the hide key is held past the threshold. Looking and
		// wheel-zoom controls are handed to the map for the duration and given back on release.
		bool inputControlledMode = false;
		void EnterInputControlledMode();
		void LeaveInputControlledMode();
		// The game's own local-map speeds, so panning feels like the vanilla local map.
		const float& localMapMousePanSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalMousePanSpeed:MapMenu")->data.f;
		const float& localMapMouseZoomSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalMouseZoomSpeed:MapMenu")->data.f;
		const float& localMapGamepadPanSpeed = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalGamepadPanSpeed:MapMenu")->data.f;

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

		// ShouldRedrawWorld's state.
		//
		// What counts as "the world changed" is deliberately NOT the player's parent cell. In an
		// exterior that pointer changes every time the player crosses a cell boundary at a run,
		// which is ordinary streaming, not a load - keying off it froze the map for the settle
		// time every few seconds of running (author report, 2026-09-02: "some kind of lag when
		// fast motion happens, though it's not every time"). What is held for is a LOAD: a
		// loading screen, a worldspace change, crossing between an interior and an exterior, or
		// the player being moved further in one frame than anything can travel.
		//
		// The worldspace is compared by pointer only and never dereferenced, so a stale one is
		// harmless; all that matters is that it differs.
		const void* lastSeenWorldSpace = nullptr;
		bool lastSeenInterior = false;
		bool hasLastPlayerPos = false;
		RE::NiPoint3 lastPlayerPos{};

		// One frame's worth of movement can never be a whole cell (4096 units) - a sprint is
		// about twenty. Anything beyond it is a coc, a cow, a fast travel or a script moving
		// the player, all of which mean the engine is about to stream a new area in.
		static constexpr float kTeleportDistance = 4096.0F;
		std::chrono::steady_clock::time_point worldChangedAt = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point lastWorldRedrawAt{};

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

		// How long the visibility gate must stay OPEN before the mod accepts it and stops
		// re-asserting. Distinct from kRequiredStableFrames above, which is about the artwork's
		// position going still - these are two different kinds of "settled" and must not share a
		// constant.
		//
		// A single frame was far too eager. A log had the gate open at 20:30:04.672, settled at
		// .694, and closed again by something else at .926 - 232ms later, long after the mod had
		// stood down for good. Sixty frames is about a second at 60fps, comfortably past that.
		static constexpr int kVisibilityStableFrames = 60;

		// True once the minimap has been visible CONTINUOUSLY for kVisibilityStableFrames. After
		// that the mod stops re-asserting visibility entirely, so the world map, the local map and
		// any other mod that hides the HUD can do so without being overridden.
		bool visibilitySettled = false;
		int  visibilityReassertCount = 0;
		int  visibilityStableFrames = 0;
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

		// localMapMargin (RELOCATION_ID(234438, 189820)) removed in 1.6.4: never read anywhere in this plugin, and
		// AE ID 189820 is absent from every Address Library database from 1.6.1130 on, so the member
		// initializer alone made CommonLib refuse to start the game on AE 1.6.1130/1170/1179.
		const bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ REL::VariantID{ 501260, 359696, 0x1E70DFC } }.get();

		bool& useMapBrightnessAndContrastBoost = *REL::Relocation<bool*>{ RELOCATION_ID(528107, 415052) }.get();
		bool& nodeFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 390865) }.get();
		bool& nodeDrawFadeEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 390866) }.get();
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 414558) }.get();
		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 414746) }.get();
	};
}