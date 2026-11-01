#include "Minimap.h"
#include "RenderDiagnostics.h"

#include "RE/B/BSTimer.h"
#include "RE/I/ImageSpaceManager.h"
#include "RE/I/ImageSpaceShaderParam.h"
#include "RE/L/LocalMapCamera.h"
#include "RE/R/Renderer.h"
#include "RE/S/ShaderAccumulator.h"
#include "RE/S/ShadowState.h"
#include "RE/R/RenderPassCache.h"
#include "RE/R/RenderTargetManager.h"

#include <cmath>
#include <numbers>

namespace RE
{
#if RUNTIME_LINE != 17  // CommonLibSSE-NG 7.2 declares BSPortalGraphEntry itself (portalGraph at +0x10, as here)
	class BSPortalGraphEntry : public NiRefObject
	{
	public:
		~BSPortalGraphEntry() override;	 // 00

		// members
		BSPortalGraph* portalGraph;	 // 10
									 // ...
	};
	//static_assert(sizeof(BSPortalGraphEntry) == 0x140);
#endif

	RE::BSPortalGraphEntry* Main__GetPortalGraphEntry(Main* a_this)
	{
		using func_t = decltype(&Main__GetPortalGraphEntry);
		REL::Relocation<func_t> func{ RELOCATION_ID(35607, 36617) };

		return func(a_this);
	}

	void* NiCamera__Accumulate(NiCamera* a_this, BSGraphics::BSShaderAccumulator* a_shaderAccumulator, std::uint32_t a_unk2)
	{
		using func_t = decltype(&NiCamera__Accumulate);
		REL::Relocation<func_t> func{ RELOCATION_ID(99789, 106436) };

		return func(a_this, a_shaderAccumulator, a_unk2);
	}
}

namespace DEM
{
	void Minimap::UpdateFogOfWar()
	{
		auto& fogOfWarOverlayHolder = reinterpret_cast< RE::NiPointer<RE::NiNode>&>(cullingProcess->GetFogOfWarOverlay());

		if (!fogOfWarOverlayHolder)
		{
			logger::debug("Fog of war overlay not created yet; creating it");
			localMap->localCullingProcess.CreateFogOfWar();

			// This pass only made the empty holder; the overlay is attached on the next one, which must not wait for the age limit.
			BumpContentEpoch(perf::EpochCause::kOther);
		}
		else
		{
			// Clear last-frame fog of war overlay because I don't know how to update BSTriShapes.
			// Maybe that would improve performance.
			std::uint32_t childrenSize = fogOfWarOverlayHolder->GetChildren().size();
			for (std::uint32_t i = 0; i < childrenSize; i++)
			{
				fogOfWarOverlayHolder->DetachChildAt(i);
			}

#if RUNTIME_LINE == 17
			line17::FogOfWar fogOfWar;
#else
			RE::LocalMapMenu::FogOfWar fogOfWar;
#endif

			fogOfWar.overlayHolder = fogOfWarOverlayHolder.get();

			RE::TESObjectCELL* interiorCell = RE::TES::GetSingleton()->interiorCell;
			bool skyCellAttached = false;

			if (interiorCell)
			{
				fogOfWar.minExtent = cameraContext->minExtent;
				fogOfWar.maxExtent = cameraContext->maxExtent;
				fogOfWar.gridDivisions = 32;
		
				cullingProcess->AttachFogOfWarOverlay(fogOfWar, interiorCell);
			}
			else
			{
				fogOfWar.gridDivisions = 16;
		
				RE::GridCellArray* gridCells = RE::TES::GetSingleton()->gridCells;
				std::uint32_t gridLength = gridCells->length;
		
				if (gridLength)
				{
					for (std::uint32_t gridX = 0; gridX < gridLength; gridX++)
					{	
						for (std::uint32_t gridY = 0; gridY < gridLength; gridY++)
						{					
							RE::TESObjectCELL* cell = gridCells->GetCell(gridX, gridY);
							if (cell && cell->IsAttached())
							{
								cullingProcess->AttachFogOfWarOverlay(fogOfWar, cell);
							}
						}
					}
				}

				RE::TESWorldSpace* worldSpace = RE::TES::GetSingleton()->GetRuntimeData2().worldSpace;
				if (worldSpace)
				{
					RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
					if (skyCell && skyCell->IsAttached())
					{
						cullingProcess->AttachFogOfWarOverlay(fogOfWar, skyCell);
						skyCellAttached = true;
					}
				}
			}

			// Runs every frame while fog of war is enabled - only log when the branch taken actually changes.
			static bool s_loggedFogOfWarState = false;
			static bool s_lastFogOfWarInterior = false;
			static bool s_lastFogOfWarSkyAttached = false;
			bool isFogOfWarInterior = interiorCell != nullptr;
			if (!s_loggedFogOfWarState || isFogOfWarInterior != s_lastFogOfWarInterior ||
				(!isFogOfWarInterior && skyCellAttached != s_lastFogOfWarSkyAttached))
			{
				if (isFogOfWarInterior)
				{
					logger::debug("Fog of war updating for interior cell {:X}", interiorCell->GetFormID());
				}
				else
				{
					logger::debug("Fog of war updating for exterior grid; sky cell overlay {}", skyCellAttached ? "attached" : "not attached");
				}

				s_loggedFogOfWarState = true;
				s_lastFogOfWarInterior = isFogOfWarInterior;
				s_lastFogOfWarSkyAttached = skyCellAttached;
			}

			fogOfWarOverlayHolder->local.translate.z = (cameraContext->maxExtent.z - cameraContext->minExtent.z) * 0.5;
			
			RE::NiUpdateData niUpdateData;
			niUpdateData.time = 0.0F;

			fogOfWarOverlayHolder->Update(niUpdateData);
		}
	}

	// FOG ON CHANGE (1.8.0). The overlay cannot be updated in place: AttachFogOfWarOverlay (16101) allocates a new shape and
	// its GPU buffers per cell on every call, and nothing in the engine edits one afterwards. But what it draws only changes
	// when the player reaches another fog square or the set of cells changes, and both of those move the content epoch. So
	// it is built when the epoch moved, and at most a second late for anything else; between builds the shapes stay
	// attached and the fog pass draws them as they are.
	void Minimap::UpdateFogOfWarIfNeeded(std::chrono::steady_clock::time_point a_now)
	{
		const std::uint32_t epoch = contentEpoch.load(std::memory_order_relaxed);

		if (settings::performance::fogOnChange && fogOverlayBuilt && epoch == fogBuiltEpoch &&
			a_now - fogBuiltAt < std::chrono::seconds(1))
		{
			fogOverlayRetained = true;
			perf::Count(perf::Outcome::kFogKept);
			return;
		}

		{
			const perf::ScopedTimer fogTimer{ perf::Section::kFog };
			UpdateFogOfWar();
		}
		perf::Count(perf::Outcome::kFogRebuilt);

		fogOverlayRetained = false;
		fogOverlayBuilt = true;
		fogBuiltAt = a_now;
		fogBuiltEpoch = contentEpoch.load(std::memory_order_relaxed);   // after the build: creating the holder bumps it, for the next pass
		if (fogBuiltEpoch != epoch)
		{
			fogOverlayBuilt = false;
		}
	}

	// A shape that was freed after one pass never had to give its render passes back. One that is drawn again does, or each
	// pass adds to its list - the same thing ClearTerrainRenderPasses is there for.
	void Minimap::ClearFogRenderPasses(RE::NiAVObject* a_object)
	{
		if (!a_object)
		{
			return;
		}

		if (RE::BSGeometry* geometry = a_object->AsGeometry())
		{
#if RUNTIME_LINE == 17
			auto shaderProp = geometry->GetGeometryRuntimeData().shaderProperty.get();
#else
			auto shaderProp = static_cast<RE::BSShaderProperty*>(geometry->properties[RE::BSGeometry::States::kEffect].get());
#endif
			if (shaderProp)
			{
				shaderProp->DoClearRenderPasses();
			}
		}

		if (RE::NiNode* node = a_object->AsNode())
		{
			for (RE::NiPointer<RE::NiAVObject>& child : node->GetChildren())
			{
				ClearFogRenderPasses(child.get());
			}
		}
	}

	// ----------------------------------------------------------------------------------------
	// WORLD STEADINESS (1.6.4). The settle window in ShouldRedrawWorld is a TIMER - 1500 ms after
	// the loading screen closes - and a timer is a guess about how long the engine needs. Two
	// crashes on 2026-09-04 (crash-2026-09-05-00-26-54 / -00-41-48, identical stacks) came from
	// RenderOffScreen -> CullJobDescriptor::Cull reading a garbage NiAVObject 1.5-3 s after a
	// `coc` from the main menu, i.e. AFTER the window had expired and while cells were still
	// streaming in. So ask the world itself, every frame, before touching its scene graph:
	// every grid cell must be attached with its 3D built, and the shadow scene children this
	// render walks must be real pointers. Anything short of that skips the redraw and keeps the
	// last picture - the player sees nothing change, and nothing is culled that is not there.
	// ----------------------------------------------------------------------------------------
	namespace
	{
		bool LooksLikePointer(const void* a_p)
		{
			return reinterpret_cast<std::uintptr_t>(a_p) > 0x10000;
		}
	}

	bool Minimap::WorldIsSteady(const char*& a_reason)
	{
		a_reason = nullptr;

		RE::TES* tes = RE::TES::GetSingleton();
		if (!tes) { a_reason = "no TES singleton"; return false; }

		if (RE::TESObjectCELL* interior = tes->interiorCell)
		{
			if (!interior->IsAttached()) { a_reason = "interior cell not attached"; return false; }
			const auto* loaded = interior->GetRuntimeData().loadedData;
			if (!loaded || !loaded->cell3D) { a_reason = "interior cell has no 3D yet"; return false; }
		}
		else
		{
			RE::TESWorldSpace* ws = tes->GetRuntimeData2().worldSpace;
			if (!ws) { a_reason = "no worldspace"; return false; }
			RE::TESObjectCELL* sky = ws->GetSkyCell();
			if (!sky || !sky->IsAttached()) { a_reason = "sky cell not attached"; return false; }

			const RE::GridCellArray* grid = tes->gridCells;
			if (!grid) { a_reason = "no cell grid"; return false; }
			for (int x = 0; x < static_cast<int>(grid->length); ++x)
			{
				for (int y = 0; y < static_cast<int>(grid->length); ++y)
				{
					RE::TESObjectCELL* cell = grid->GetCell(x, y);
					if (!cell) { continue; }                       // an empty slot is fine
					if (!cell->IsAttached()) { a_reason = "a grid cell is still attaching"; return false; }
					const auto* loaded = cell->GetRuntimeData().loadedData;
					if (!loaded || !loaded->cell3D) { a_reason = "a grid cell has no 3D yet"; return false; }
				}
			}
		}

		// THE SHADOW SCENE HAS A DIFFERENT SHAPE IN INTERIORS - PROVEN, NOT ASSUMED.
		//
		// AuroraSake (Nexus, 2026-09-15) reported the map mostly not drawing at all in interiors. The
		// 1.6.6 diagnostic answered it on the first interior visit:
		//
		//     Shadow-scene gate: child[8] is 0x0 (capacity 10, size 6, interior yes)
		//
		// An interior's shadow scene holds SIX children. The old test demanded valid pointers at
		// indices 3, 8 and 9 and gated on capacity() - the allocation, 10 - rather than size(). So in
		// any interior it read two slots past the end, found null, and declared the world unsteady.
		// That restarts the settle window every frame, so the redraw is held FOR EVER: in the test run
		// "World is steady again" never appeared once. Exteriors have enough children that the same
		// test passes, which is why this only ever bit interiors.
		//
		// Now: gate on size(), and validate only the indices that exist. The point of the check is that
		// the children this render walks are real objects before it walks them - a child that is not
		// there is not a child that is half-built, and must not fail the world.
#if RUNTIME_LINE == 17
		RE::ShadowSceneNode* shadow = line17::GetMainShadowSceneNode();
#else
		RE::ShadowSceneNode* shadow = RE::ShadowSceneNode::GetMain();
#endif
		if (!shadow) { a_reason = "no main shadow scene node"; return false; }
		auto& children = shadow->GetChildren();

		const std::size_t childCount = children.size();
		if (childCount == 0) { a_reason = "shadow scene has no children yet"; return false; }

		for (const std::size_t idx : { std::size_t{ 3 }, std::size_t{ 8 }, std::size_t{ 9 } })
		{
			if (idx >= childCount) { continue; }   // not present in this scene shape (interiors have 6)
			if (!LooksLikePointer(children[idx].get()))
			{
				a_reason = "a shadow scene child is not a valid object yet";
				return false;
			}
		}

		return true;
	}

	bool Minimap::ShouldRedrawWorld()
	{
		using clock = std::chrono::steady_clock;

		const clock::time_point now = clock::now();

		const RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		const RE::TES* tes = RE::TES::GetSingleton();

		const void* worldSpace = tes ? static_cast<const void*>(tes->GetRuntimeData2().worldSpace) : nullptr;
		const bool interior = tes && tes->interiorCell != nullptr;
		const RE::NiPoint3 playerPos = player ? player->GetPosition() : RE::NiPoint3{};

		// A load, not ordinary movement - see the members these compare against for why the
		// parent cell is deliberately not one of these tests.
		const char* reason = nullptr;

		if (worldSpace != lastSeenWorldSpace)
		{
			reason = "worldspace changed";
		}
		else if (interior != lastSeenInterior)
		{
			reason = "moved between an interior and an exterior";
		}
		else if (hasLastPlayerPos && lastPlayerPos.GetDistance(playerPos) > kTeleportDistance)
		{
			reason = "the player was moved further than one frame of travel";
		}

		lastSeenWorldSpace = worldSpace;
		lastSeenInterior = interior;
		lastPlayerPos = playerPos;
		hasLastPlayerPos = true;

		if (reason)
		{
			worldChangedAt = now;

			logger::debug("World changed ({}); holding the map redraw for {} ms while it settles",
				reason, settings::rendering::settleMs);
		}

		if (settings::rendering::skipWhileWorldSettles)
		{
			// A loading screen is the clearest "not steady" signal there is, and the settle
			// window has to start when it ENDS, not when the change behind it was noticed.
			RE::UI* ui = RE::UI::GetSingleton();
			if (ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
			{
				worldChangedAt = now;
				return false;
			}

			const auto sinceChange = std::chrono::duration_cast<std::chrono::milliseconds>(now - worldChangedAt).count();
			if (sinceChange < settings::rendering::settleMs)
			{
				return false;
			}
		}

		// Structural gate - the timer above says "probably settled"; this says "actually built".
		{
			const char* why = nullptr;
			const bool steady = WorldIsSteady(why);
			static bool s_lastSteady = true;
			if (steady != s_lastSteady)
			{
				if (steady) { logger::debug("World is steady again; map redraws resume"); }
				else { logger::debug("World not steady ({}); holding the map redraw and restarting the settle window", why ? why : "?"); }
				s_lastSteady = steady;
			}
			if (!steady)
			{
				worldChangedAt = now;   // the settle window restarts from the moment the world is whole
				return false;
			}
		}

		if (settings::rendering::redrawIntervalMs > 0)
		{
			const auto sinceRedraw = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWorldRedrawAt).count();
			if (sinceRedraw < settings::rendering::redrawIntervalMs)
			{
				return false;
			}
		}

		lastWorldRedrawAt = now;
		return true;
	}

	// ----------------------------------------------------------------------------------------
	// PICTURE REUSE (1.8.0).
	//
	// What makes this safe to skip: kLOCAL_MAP is an engine render target that keeps its contents until something draws
	// into it. Minimap.swf bound it once, in LocalMap.as InitMap (loadClip "img://Local_Map"), and nothing per frame
	// re-binds it, so the HUD movie goes on showing whatever the target holds. The settle gate and iRedrawIntervalMs have
	// always relied on the same fact. What can write it behind our back is the game's own map menu (the target is
	// shared) - hence the epoch bump on a menu closing, and kPictureGap for a stretch with no pass at all.
	//
	// Why the markers stay right: they are placed from the live camera every frame in Advance(). A picture is only
	// reused while the camera is within about a pixel of where it was drawn, so the two cannot visibly part.
	// ----------------------------------------------------------------------------------------
	namespace
	{
		constexpr float kYawTolerance = 0.25F * std::numbers::pi_v<float> / 180.0F;   // 0.25 degrees
		constexpr float kZTolerance = 16.0F;                                          // world units; the camera looks straight down
		constexpr float kZoomTolerance = 0.0005F;
		constexpr float kExteriorFogSquare = 256.0F;                                  // a 4096-unit cell in 16 grid divisions
		constexpr std::int32_t kInteriorFogDivisions = 32;

		float AngleBetween(float a_a, float a_b)
		{
			constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;
			float d = std::fmod(std::abs(a_a - a_b), kTwoPi);
			return d > std::numbers::pi_v<float> ? kTwoPi - d : d;
		}

		bool Near(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, float a_tolerance)
		{
			return std::abs(a_a.x - a_b.x) < a_tolerance && std::abs(a_a.y - a_b.y) < a_tolerance && std::abs(a_a.z - a_b.z) < a_tolerance;
		}

		// Game events only move the epoch counter. Nothing here touches Scaleform or the scene graph (past-mistakes 12737).
		class PictureEventSink final :
			public RE::BSTEventSink<RE::TESCellAttachDetachEvent>,
			public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
			public RE::BSTEventSink<RE::TESActivateEvent>,
			public RE::BSTEventSink<RE::TESDestructionStageChangedEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent*, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
			{
				Minimap::BumpContentEpoch(perf::EpochCause::kCell);
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent*, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override
			{
				Minimap::BumpContentEpoch(perf::EpochCause::kCell);
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override
			{
				RE::TESBoundObject* base = a_event && a_event->objectActivated ? a_event->objectActivated->GetBaseObject() : nullptr;
				if (base)
				{
					if (base->Is(RE::FormType::Door))
					{
						Minimap::BumpContentEpoch(perf::EpochCause::kDoor);
					}
					else if (const auto* destructible = base->As<RE::BGSDestructibleObjectForm>(); destructible && destructible->data)
					{
						Minimap::BumpContentEpoch(perf::EpochCause::kDestructible);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESDestructionStageChangedEvent*, RE::BSTEventSource<RE::TESDestructionStageChangedEvent>*) override
			{
				Minimap::BumpContentEpoch(perf::EpochCause::kDestructible);
				return RE::BSEventNotifyControl::kContinue;
			}

			// ANY menu closing, not a list of names: every menu that renders the local map shares kLOCAL_MAP with us, the
			// list is not ours to know (other mods add menus), and one extra redraw per closed menu costs nothing.
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening)
				{
					Minimap::BumpContentEpoch(perf::EpochCause::kMenuClosed);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Minimap::BumpContentEpoch(perf::EpochCause a_cause)
	{
		contentEpoch.fetch_add(1, std::memory_order_relaxed);
		if (a_cause == perf::EpochCause::kCell || a_cause == perf::EpochCause::kDoor ||
			a_cause == perf::EpochCause::kDestructible || a_cause == perf::EpochCause::kSettings)
		{
			cacheEpoch.fetch_add(1, std::memory_order_relaxed);
		}
		perf::Count(a_cause);
	}

	void Minimap::RegisterPictureEventSinks()
	{
		static PictureEventSink sink;
		static bool registered = false;
		if (registered)
		{
			return;
		}

		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		auto* ui = RE::UI::GetSingleton();
		if (!holder || !ui)
		{
			logger::warn("Picture events: {} is not available; the picture refreshes on its age limit alone",
				!holder ? "the script event holder" : "the UI");
			return;
		}

		holder->GetEventSource<RE::TESCellAttachDetachEvent>()->AddEventSink(&sink);
		holder->GetEventSource<RE::TESCellFullyLoadedEvent>()->AddEventSink(&sink);
		holder->GetEventSource<RE::TESActivateEvent>()->AddEventSink(&sink);
		holder->GetEventSource<RE::TESDestructionStageChangedEvent>()->AddEventSink(&sink);
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&sink);

		registered = true;
		logger::debug("Picture events registered: cell attach/detach and load, activation, destruction stage, menu close");
	}

	bool Minimap::BuildPictureKey(PictureKey& a_key)
	{
		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		RE::PlayerCamera* playerCamera = RE::PlayerCamera::GetSingleton();
		RE::TES* tes = RE::TES::GetSingleton();
		if (!player || !playerCamera || !tes || !cameraContext || !cameraContext->defaultState || !cameraContext->camera ||
			!GetPixelShaderProperties)
		{
			return false;
		}

		a_key.player = player->GetPosition();

		// Turning changes the picture only when the map turns with the camera. With north up the angle is the parent
		// cell's, which is constant in a worldspace and part of the cell events anyway.
		a_key.followYaw = settings::controls::followPlayerCameraRotation;
		a_key.yaw = a_key.followYaw ? playerCamera->GetRuntimeData2().yaw : 0.0F;

		a_key.pan = cameraContext->defaultState->translation;   // hold-to-pan moves the camera with the player standing still
		a_key.zoom = cameraContext->defaultState->zoom;

		const auto& frustum = cameraContext->camera->GetRuntimeData2().viewFrustum;
		a_key.frustumWidth = frustum.fRight - frustum.fLeft;
		a_key.frustumHeight = frustum.fTop - frustum.fBottom;

		a_key.boundsA = cameraContext->minExtent;
		a_key.boundsB = cameraContext->maxExtent;

		a_key.shape = shape;
		GetPixelShaderProperties(a_key.lmuShape, a_key.lmuStyle);
		a_key.fogEnabled = isFogOfWarEnabled;

		a_key.worldSpace = tes->GetRuntimeData2().worldSpace;
		RE::TESObjectCELL* interior = tes->interiorCell;
		a_key.interiorCell = interior ? interior->GetFormID() : 0;

#if RUNTIME_LINE == 17
		RE::BSGraphics::RenderTargetManager* renderTargetManager = line17::RenderTargetManager();
#else
		RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();
#endif
		if (renderTargetManager)
		{
			const auto& target = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP];
			a_key.targetWidth = target.width;
			a_key.targetHeight = target.height;
		}

		// Crossing a fog square is what can reveal more of the map. Interiors divide the mapped area instead of a cell.
		if (a_key.fogEnabled)
		{
			float squareX = kExteriorFogSquare;
			float squareY = kExteriorFogSquare;
			if (interior)
			{
				squareX = std::max(std::abs(a_key.boundsB.x - a_key.boundsA.x) / kInteriorFogDivisions, 1.0F);
				squareY = std::max(std::abs(a_key.boundsB.y - a_key.boundsA.y) / kInteriorFogDivisions, 1.0F);
			}

			const auto fogX = static_cast<std::int32_t>(std::floor(a_key.player.x / squareX));
			const auto fogY = static_cast<std::int32_t>(std::floor(a_key.player.y / squareY));
			if (hasFogSquare && (fogX != lastFogSquareX || fogY != lastFogSquareY))
			{
				BumpContentEpoch(perf::EpochCause::kFogGrid);
			}
			hasFogSquare = true;
			lastFogSquareX = fogX;
			lastFogSquareY = fogY;
		}

		a_key.epoch = contentEpoch.load(std::memory_order_relaxed);
		return true;
	}

	bool Minimap::PictureStillGood(const PictureKey& a_key, perf::Reason& a_why) const
	{
		const PictureKey& last = lastPictureKey;

		if (a_key.epoch != last.epoch)
		{
			a_why = perf::Reason::kEpoch;
			return false;
		}

		if (a_key.worldSpace != last.worldSpace || a_key.interiorCell != last.interiorCell ||
			a_key.targetWidth != last.targetWidth || a_key.targetHeight != last.targetHeight)
		{
			a_why = perf::Reason::kWorld;
			return false;
		}

		if (a_key.shape != last.shape || a_key.lmuShape != last.lmuShape || a_key.lmuStyle != last.lmuStyle ||
			a_key.fogEnabled != last.fogEnabled || a_key.followYaw != last.followYaw)
		{
			a_why = perf::Reason::kShapeStyle;
			return false;
		}

		// About one pixel of the picture, in world units, at the zoom it was drawn with. An engine frustum that cannot be
		// read yet (0 wide) gives the floor, which only means redrawing more than necessary.
		const float pixel = a_key.targetWidth > 0 ? std::clamp(a_key.frustumWidth / static_cast<float>(a_key.targetWidth), 0.25F, 64.0F) : 0.25F;

		// Runs every frame - logged only when the size the position test works to changes (a zoom, a new area).
		static float s_lastLoggedPixel = -1.0F;
		if (std::abs(pixel - s_lastLoggedPixel) > 0.5F)
		{
			logger::debug("Picture reuse: one pixel of the {}x{} picture is {:.2f} world units (frustum {:.0f} wide)",
				a_key.targetWidth, a_key.targetHeight, pixel, a_key.frustumWidth);
			s_lastLoggedPixel = pixel;
		}

		if (std::abs(a_key.zoom - last.zoom) >= kZoomTolerance || std::abs(a_key.frustumWidth - last.frustumWidth) >= pixel ||
			std::abs(a_key.frustumHeight - last.frustumHeight) >= pixel || !Near(a_key.boundsA, last.boundsA, 1.0F) ||
			!Near(a_key.boundsB, last.boundsB, 1.0F))
		{
			a_why = perf::Reason::kZoomBounds;
			return false;
		}

		if (std::abs(a_key.player.x - last.player.x) >= pixel || std::abs(a_key.player.y - last.player.y) >= pixel ||
			std::abs(a_key.player.z - last.player.z) >= kZTolerance || !Near(a_key.pan, last.pan, pixel))
		{
			a_why = perf::Reason::kPosition;
			return false;
		}

		if (a_key.followYaw && AngleBetween(a_key.yaw, last.yaw) >= kYawTolerance)
		{
			a_why = perf::Reason::kRotation;
			return false;
		}

		return true;
	}

	bool Minimap::RenderOffScreen(PassMode a_mode, const PictureKey* a_composeKey)
	{
		LMU::PixelShaderProperty::Shape prevShaderShape;
		LMU::PixelShaderProperty::Style shaderStyle;

		// Local Map Upgrade hands these over in its kPixelShaderPropertiesHook message, which it
		// dispatches from its own kDataLoaded handler. SKSE delivers kDataLoaded in load order, so
		// whether they have arrived at any given earlier moment depends on which plugin loaded
		// first - it is not something to assert on. They are reliably present by the time anything
		// is actually rendered, which is here. Checked rather than assumed all the same, per
		// CLAUDE.md rule 14 (debug logging and null checks together) and rule 17 (retry a lookup
		// that can fail because something is not ready yet - a not-ready pointer is not an error,
		// it just means not yet).
		if (!GetPixelShaderProperties || !SetPixelShaderProperties)
		{
			// Per-frame path, so this logs once rather than every frame.
			static bool warnedMissingShaderHooks = false;
			if (!warnedMissingShaderHooks)
			{
				logger::warn("RenderOffScreen: Local Map Upgrade's pixel-shader hooks are not available yet; drawing without the shape swap this frame");
				warnedMissingShaderHooks = true;
			}

			return false;
		}

		GetPixelShaderProperties(prevShaderShape, shaderStyle);
		SetPixelShaderProperties(shape, shaderStyle);

		// Runs every frame - only log the shader shape swap when it actually differs from last frame.
		static bool s_loggedShapeSwap = false;
		static LMU::PixelShaderProperty::Shape s_lastLoggedPrevShape{};
		static LMU::PixelShaderProperty::Shape s_lastLoggedTargetShape{};
		if (!s_loggedShapeSwap || prevShaderShape != s_lastLoggedPrevShape || shape != s_lastLoggedTargetShape)
		{
			logger::debug("Off-screen render: swapping pixel shader shape from {} to {} to draw the minimap, restoring after",
				prevShaderShape == LMU::PixelShaderProperty::Shape::kRound ? "round" : "squared",
				shape == LMU::PixelShaderProperty::Shape::kRound ? "round" : "squared");
			s_loggedShapeSwap = true;
			s_lastLoggedPrevShape = prevShaderShape;
			s_lastLoggedTargetShape = shape;
		}

		// 1. Setup culling step ///////////////////////////////////////////////////////////////////////////////////////////

		useMapBrightnessAndContrastBoost = true;

#if RUNTIME_LINE == 17
		RE::ShadowSceneNode* mainShadowSceneNode = line17::GetMainShadowSceneNode();
#else
		RE::ShadowSceneNode* mainShadowSceneNode = RE::ShadowSceneNode::GetMain();
#endif

        RE::NiPointer<RE::BSGraphics::BSShaderAccumulator>& shaderAccumulator = cullingProcess->GetShaderAccumulator();

		shaderAccumulator->GetRuntimeData().activeShadowSceneNode = mainShadowSceneNode;

		RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& mainShadowSceneChildren = mainShadowSceneNode->GetChildren();

		bool isLightUpdateDisabled = mainShadowSceneNode->GetRuntimeData().disableLightUpdate;
		mainShadowSceneNode->GetRuntimeData().disableLightUpdate = true;

		RE::NiPointer<RE::NiAVObject>& objectLODRoot = mainShadowSceneChildren[3];
		bool areLODsHidden = objectLODRoot->GetFlags().any(RE::NiAVObject::Flag::kHidden);
		objectLODRoot->GetFlags().reset(RE::NiAVObject::Flag::kHidden);

		bool isByte_1431D1D30 = byte_1431D1D30;
		bool isNodeFadeEnabled = nodeFadeEnabled;
		byte_1431D1D30 = true;
		nodeDrawFadeEnabled = nodeFadeEnabled = false;
		dword_1431D0D8C = 0;

		RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
#if RUNTIME_LINE == 17
		line17::SetClearColor(renderer, 0.0F, 0.0F, 0.0F, 1.0F);
#else
		renderer->SetClearColor(0.0F, 0.0F, 0.0F, 1.0F);
#endif

        RE::TES* tes = RE::TES::GetSingleton();
		RE::TESWorldSpace* worldSpace = tes->GetRuntimeData2().worldSpace;

#if RUNTIME_LINE == 17
		line17::LocalMapCullingProcess::UnkData unkData{ cullingProcess };
#else
		RE::LocalMapMenu::LocalMapCullingProcess::UnkData unkData{ cullingProcess };
#endif
		
		if (worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions))
		{
			unkData.unk8 = false;
		}
		else
		{
			unkData.unk8 = true;
		}

		// Runs every frame - only log when unk8 actually changes (e.g. on a worldspace change).
		static bool s_loggedUnk8 = false;
		static bool s_lastUnk8 = false;
		if (!s_loggedUnk8 || unkData.unk8 != s_lastUnk8)
		{
			logger::debug("Worldspace has fixed dimensions: {}; culling unk8 set to {}",
				worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions), unkData.unk8);
			s_loggedUnk8 = true;
			s_lastUnk8 = unkData.unk8;
		}

		// 2. Culling step /////////////////////////////////////////////////////////////////////////////////////////////////
		// Compose has nothing to cull but the fog overlay, below.

		if (a_mode != PassMode::kCompose)
		{
		RE::TESObjectCELL* currentCell = nullptr;

		if (RE::TESObjectCELL* interiorCell = tes->interiorCell)
		{
			currentCell = interiorCell;
		}
		else if (worldSpace)
		{
			RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
			if (skyCell && skyCell->IsAttached())
            {
				currentCell = skyCell;

				CullTerrain(tes->gridCells, unkData, nullptr);
            }
		}

		// Runs every frame - only log when which cell we're culling against actually changes.
		static bool s_loggedCellCategory = false;
		static int s_lastCellCategory = -1;
		int cellCategory = currentCell ? (currentCell == tes->interiorCell ? 0 : 1) : 2;
		if (!s_loggedCellCategory || cellCategory != s_lastCellCategory)
		{
			switch (cellCategory)
			{
			case 0:
				logger::debug("Off-screen render culling interior cell {:X}", currentCell->GetFormID());
				break;
			case 1:
				logger::debug("Off-screen render culling exterior sky cell {:X}", currentCell->GetFormID());
				break;
			default:
				logger::debug("Off-screen render has no resolvable cell to cull (no interior cell, no attached sky cell)");
				break;
			}
			s_loggedCellCategory = true;
			s_lastCellCategory = cellCategory;
		}

        if (currentCell)
        {
			cullingProcess->CullCellObjects(unkData, currentCell);
		}
		}

#if RUNTIME_LINE == 17
        line17::CullJobDescriptor& cullJobDesc = cullingProcess->cullJobDesc;
#else
        RE::CullJobDescriptor& cullJobDesc = cullingProcess->cullJobDesc;
#endif
		RE::NiPointer<RE::NiCamera> camera = cameraContext->camera;

        cullJobDesc.camera = camera;

		if (a_mode != PassMode::kCompose)
		{
        RE::BSPortalGraphEntry* portalGraphEntry = RE::Main__GetPortalGraphEntry(RE::Main::GetSingleton());
		
		// Edge-triggered: this runs every frame, but a missing portal graph would mean degraded
		// culling every frame it stays missing, so only log the transition in and out of that state.
		static bool s_portalCullingDegraded = false;

        if (portalGraphEntry)
        {
			RE::BSPortalGraph* portalGraph = portalGraphEntry->portalGraph;
            if (portalGraph)
            {
#if RUNTIME_LINE == 17
				// 7.2 names BSPortalGraph+0x58 alwaysRenderChildren (a BSTArray<NiPointer<NiAVObject>>) - the member line 1 calls unk58.
				static_assert(offsetof(RE::BSPortalGraph, alwaysRenderChildren) == 0x58);
				cullJobDesc.cullingObjects = &portalGraph->alwaysRenderChildren;
#else
				cullJobDesc.cullingObjects = reinterpret_cast<RE::BSTArray<RE::NiPointer<RE::NiAVObject>>*>(&portalGraph->unk58);
#endif
				cullJobDesc.Cull(1, 0);

				if (s_portalCullingDegraded)
				{
					logger::debug("Portal graph culling data available again");
					s_portalCullingDegraded = false;
				}
            }
			else if (!s_portalCullingDegraded)
			{
				logger::warn("Portal graph entry has no portal graph; skipping portal-object culling");
				s_portalCullingDegraded = true;
			}
        }
		else if (!s_portalCullingDegraded)
		{
			logger::warn("Could not get the portal graph entry; skipping portal-object culling");
			s_portalCullingDegraded = true;
		}

        if (mainShadowSceneChildren.capacity() > 9 && LooksLikePointer(mainShadowSceneChildren[9].get()))
        {
			RE::NiPointer<RE::NiAVObject>& portalSharedNode = mainShadowSceneChildren[9];
			cullJobDesc.scene = portalSharedNode;
        }
		else
		{
			cullJobDesc.scene = nullptr;

			// One-shot: the shadow scene's child count shouldn't shrink back once the engine has
			// initialized it, so don't keep re-logging this every frame if it stays missing.
			static bool s_loggedPortalSharedMissing = false;
			if (!s_loggedPortalSharedMissing)
			{
				logger::warn("Shadow scene has no portal-shared node (children capacity {}); skipping its culling",
					mainShadowSceneChildren.capacity());
				s_loggedPortalSharedMissing = true;
			}
		}
		cullJobDesc.Cull(0, 0);

        if (mainShadowSceneChildren.capacity() > 8)
        {
			RE::NiPointer<RE::NiAVObject>& multiBoundNode = mainShadowSceneChildren[8];
			cullJobDesc.scene = multiBoundNode;
        }
		else
		{
			cullJobDesc.scene = nullptr;

			static bool s_loggedMultiBoundMissing = false;
			if (!s_loggedMultiBoundMissing)
			{
				logger::warn("Shadow scene has no multibound node (children capacity {}); skipping its culling",
					mainShadowSceneChildren.capacity());
				s_loggedMultiBoundMissing = true;
			}
		}
		cullJobDesc.Cull(0, 0);
		}

		// 3. Rendering step ///////////////////////////////////////////////////////////////////////////////////////////////

#if RUNTIME_LINE == 17
        // The manager is the static object at 524970 / 411451 itself, as the game passes it (Line17.h).
        RE::BSGraphics::RenderTargetManager* renderTargetManager = line17::RenderTargetManager();

        int depthStencil = line17::GetDepthStencil(renderTargetManager);
		line17::SetupDepthStencilAt(renderTargetManager, depthStencil, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, 0, false);
		if (a_mode != PassMode::kCompose)   // a clear is deferred to the next engine draw, which would wipe the stored picture
		{
			line17::SetupRenderTargetAt(renderTargetManager, 0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, true);
		}
#else
        RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

        int depthStencil = renderTargetManager->GetDepthStencil();
		renderTargetManager->SetupDepthStencilAt(depthStencil, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, 0, false);
		if (a_mode != PassMode::kCompose)   // a clear is deferred to the next engine draw, which would wipe the stored picture
		{
			renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR, true);
		}
#endif

		bool composed = true;
		if (a_mode == PassMode::kCompose)
		{
			composed = a_composeKey && ComposeInteriorPicture(*a_composeKey);
		}
		else
		{
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);
		}

		// 4. Post process step (Add fog of war) ///////////////////////////////////////////////////////////////////////////

		// Runs every frame - only log when the fog-of-war compositing step turns on or off.
		static bool s_loggedFogCompositeState = false;
		static bool s_lastFogCompositeEnabled = false;
		if (!s_loggedFogCompositeState || isFogOfWarEnabled != s_lastFogCompositeEnabled)
		{
			logger::debug("Fog of war compositing {}", isFogOfWarEnabled ? "enabled for this frame" : "disabled; skipping compositing step");
			s_loggedFogCompositeState = true;
			s_lastFogCompositeEnabled = isFogOfWarEnabled;
		}

		// A capture stores geometry only. The fog stays a live layer, drawn over the picture on every compose.
		if (isFogOfWarEnabled && a_mode != PassMode::kCapture)
		{
#if RUNTIME_LINE == 17
			line17::ClearActiveRenderPasses(shaderAccumulator.get(), false);

			line17::SetRenderMode(19);
#else
			shaderAccumulator->ClearActiveRenderPasses(false);

			RE::BSGraphics::BSShaderAccumulator::SetRenderMode(19);
#endif

			RE::NiPointer<RE::NiAVObject> fogOfWarOverlayHolder = cullingProcess->GetFogOfWarOverlay();
			if (fogOverlayRetained)
			{
				ClearFogRenderPasses(fogOfWarOverlayHolder.get());
			}
			cullJobDesc.scene = fogOfWarOverlayHolder;
			cullJobDesc.Cull(0, 0);

			RE::BSGraphics::RendererShadowState* rendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

#if RUNTIME_LINE == 17
			line17::SetAlphaBlendWriteMode(rendererShadowState, 8);
			line17::SetDepthStencilDepthMode(rendererShadowState, RE::BSGraphics::DepthStencilDepthMode::kDisabled);

			line17::SetupRenderTargetAt(renderTargetManager, 0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, true);
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

			line17::SetDepthStencilDepthMode(rendererShadowState, RE::BSGraphics::DepthStencilDepthMode::kTestWrite);
			line17::SetAlphaBlendWriteMode(rendererShadowState, 1);
#else
			rendererShadowState->SetAlphaBlendWriteMode(8);
			rendererShadowState->SetDepthStencilDepthMode(RE::BSGraphics::DepthStencilDepthMode::kDisabled);

			renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, true);
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

			rendererShadowState->SetDepthStencilDepthMode(RE::BSGraphics::DepthStencilDepthMode::kTestWrite);
			rendererShadowState->SetAlphaBlendWriteMode(1);
#endif
		}

		// 5. Finish rendering and dispatch ////////////////////////////////////////////////////////////////////////////////

		if (a_mode != PassMode::kCapture)
		{
			rdiag::AfterGeometry();   // 1.7.5 diagnostics: what the pass inherits, and kLOCAL_MAP_SWAP before the composite on request
		}

#if RUNTIME_LINE == 17
        line17::SetupDepthStencilAt(renderTargetManager, -1, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, 0, false);
#else
        renderTargetManager->SetupDepthStencilAt(-1, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, 0, false);
#endif

		// A capture stops here: kLOCAL_MAP_SWAP holds the tile, and kLOCAL_MAP keeps the picture on screen. A failed compose
		// leaves it too - better last frame's picture than an empty one.
		if (a_mode != PassMode::kCapture && composed)
		{
		RE::ImageSpaceShaderParam& imageSpaceShaderParam = cullingProcess->GetImageSpaceShaderParam();

		RE::BSGraphics::RenderTargetProperties& renderLocalMapSwapData = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];

		float localMapSwapWidth = renderLocalMapSwapData.width;
		float localMapSwapHeight = renderLocalMapSwapData.height;

#if RUNTIME_LINE == 17
		line17::SetupPixelConstantGroup(&imageSpaceShaderParam, 0, static_cast<float>(1.0 / localMapSwapWidth), static_cast<float>(1.0 / localMapSwapHeight), 0.0F, 0.0F);
#else
		imageSpaceShaderParam.SetupPixelConstantGroup(0, 1.0 / localMapSwapWidth, 1.0 / localMapSwapHeight, 0.0, 0.0);
#endif
		
        RE::ImageSpaceManager* imageSpaceManager = RE::ImageSpaceManager::GetSingleton();

#if RUNTIME_LINE == 17
		line17::CopyWithImageSpaceEffect(imageSpaceManager, RE::ImageSpaceManager::ImageSpaceEffectEnum::ISLocalMap,
										 RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::RENDER_TARGET::kLOCAL_MAP,
										 &imageSpaceShaderParam);
#else
		imageSpaceManager->CopyWithImageSpaceEffect(RE::ImageSpaceManager::ImageSpaceEffectEnum::ISLocalMap,
													RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::RENDER_TARGET::kLOCAL_MAP,
													&imageSpaceShaderParam);
#endif
		rdiag::AfterComposite();   // 1.7.5 diagnostics: kLOCAL_MAP after the composite, on request
		}

		if (areLODsHidden)
		{
			objectLODRoot->GetFlags().set(RE::NiAVObject::Flag::kHidden);
		}

		mainShadowSceneNode->GetRuntimeData().disableLightUpdate = isLightUpdateDisabled;
		byte_1431D1D30 = isByte_1431D1D30;
		nodeDrawFadeEnabled = nodeFadeEnabled = isNodeFadeEnabled;
		dword_1431D0D8C = 0;

#if RUNTIME_LINE == 17
        line17::ClearActiveRenderPasses(shaderAccumulator.get(), false);
#else
        shaderAccumulator->ClearActiveRenderPasses(false);
#endif

		useMapBrightnessAndContrastBoost = false;

		SetPixelShaderProperties(prevShaderShape, shaderStyle);

		return composed;
	}

	// Terrain render passes can be allocated multiple times but only cleared once per frame.
	// I don't understand why only terrain render passes work this way, but once Draw is called
	// before entering my code, make sure we clear them so there is no memory leakage.
	void Minimap::ClearTerrainRenderPasses(RE::NiPointer<RE::NiAVObject>& a_object)
	{
		RE::NiNode* node = a_object->AsNode();

		if (!node)
		{
			return;
		}

		for (RE::NiPointer<RE::NiAVObject>& object : node->children) if (object)
		{
			if (object->flags.any(RE::NiAVObject::Flag::kRenderUse))
			{
				if (RE::BSGeometry* geometry = object->AsGeometry())
				{
#if RUNTIME_LINE == 17
					// 7.2 names the geometry's effect slot (properties[kEffect] on line 1, BSGeometry+0x128) shaderProperty.
					static_assert(offsetof(RE::BSGeometry::GEOMETRY_RUNTIME_DATA, shaderProperty) == 0x08);  // + the 0x120 accessor
					auto shaderProp = geometry->GetGeometryRuntimeData().shaderProperty.get();
#else
					auto shaderProp = static_cast<RE::BSShaderProperty*>(geometry->properties[RE::BSGeometry::States::kEffect].get());
#endif
					if (shaderProp)
					{
						shaderProp->DoClearRenderPasses();
					}
				}
			}

			ClearTerrainRenderPasses(object);
		}
	};

#if RUNTIME_LINE == 17
	void Minimap::CullTerrain(const RE::GridCellArray* a_gridCells, line17::LocalMapCullingProcess::UnkData& a_unkData,
							  const RE::TESObjectCELL* a_cell)
	{
		line17::CullJobDescriptor& cullJobDesc = a_unkData.ptr->cullJobDesc;
#else
	void Minimap::CullTerrain(const RE::GridCellArray* a_gridCells, RE::LocalMapMenu::LocalMapCullingProcess::UnkData& a_unkData,
							  const RE::TESObjectCELL* a_cell)
	{
		RE::CullJobDescriptor& cullJobDesc = a_unkData.ptr->cullJobDesc;
#endif

		// Called once per frame while in an exterior worldspace with an attached sky cell - only
		// log when the grid size or the unk8-gated index-2 pass actually changes, not every call.
		static bool s_loggedTerrainCullParams = false;
		static std::uint32_t s_lastGridLength = 0;
		static bool s_lastTerrainUnk8 = false;
		if (!s_loggedTerrainCullParams || a_gridCells->length != s_lastGridLength || a_unkData.unk8 != s_lastTerrainUnk8)
		{
			logger::debug("Culling terrain across a {0}x{0} grid (index-2 render passes {1})",
				a_gridCells->length, a_unkData.unk8 ? "included" : "excluded");
			s_loggedTerrainCullParams = true;
			s_lastGridLength = a_gridCells->length;
			s_lastTerrainUnk8 = a_unkData.unk8;
		}

		for (int gridCellX = 0; gridCellX < a_gridCells->length; gridCellX++)
		{
			for (int gridCellY = 0; gridCellY < a_gridCells->length; gridCellY++)
			{
				RE::TESObjectCELL* cell = a_gridCells->GetCell(gridCellX, gridCellY);
				if (cell && cell->IsAttached())
				{
					for (int sceneIndex : { 2, 3, 6, 7 }) if (sceneIndex != 2 || a_unkData.unk8)
					{
						if (RE::NiPointer<RE::NiNode> cell3D = cell->GetRuntimeData().loadedData->cell3D)
						{
							RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& cellScenes = cell3D->GetChildren();

							if (sceneIndex < cellScenes.size())
							{
								if (RE::NiPointer<RE::NiAVObject> scene = cellScenes[sceneIndex])
								{
									if (sceneIndex == 2)
									{
										ClearTerrainRenderPasses(scene);
									}

									bool isCellSceneHidden = scene->flags.any(RE::NiAVObject::Flag::kHidden);
									scene->flags.reset(RE::NiAVObject::Flag::kHidden);

									cullJobDesc.scene = scene;

									cullJobDesc.Cull(0, 0);

									if (isCellSceneHidden)
									{
										scene->flags.set(RE::NiAVObject::Flag::kHidden);
									}
								}
							}
						}
					}
				}
			}
		}

		cullJobDesc.scene = nullptr;
	}
}